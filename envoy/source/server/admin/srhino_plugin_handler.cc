#include "source/server/admin/srhino_plugin_handler.h"

#include "envoy/admin/v3/srhino_plugin_dump.pb.h"
#include "source/common/common/matchers.h"
#include "source/common/http/headers.h"
#include "source/common/http/utility.h"
#include "source/common/srhino_plugin_framework/plugin_file_manager.h"
#include "source/server/admin/utils.h"
#include "envoy/srhino_plugin_framework/v1_0_x/call_result.h"

namespace Envoy {
namespace Server {

/**
 *E.g. "/srhino_plugin/listener_name/www.srhino.com.acl.so.1.0.0.123456/168/aaa/bbb/ccc/ddd"
 *       ╰-----┬-----╯ ╰-----┬-----╯ ╰----------------┬---------------╯ ╰┬╯ ╰------┬------╯
 *          prefix     listener name            filter name              |  controller path
 *                                                                 filter epoch
 * prefix: URL标识符，固定为"srhino_plugin"
 * listener name: 监听器（网关）名称
 * filter name: composite插件下发的被包装的插件名称，包含插件实例ID
 * filter epoch: epoch id，这是可选的。当url中不包含epoch id，则自动选择当前最大的（其余实际已经
 * 失效，但是排水机制延迟销毁）
 * controller path: 插件controller的路径
 */
const std::regex SrhinoPluginHandler::url_regex_(
    R"(^/srhino_plugin/([^/]+)/([^/]+)/(((\d+)/(.+))|([A-Za-z]+.*))$)");

// url_regex_各个捕获组的索引
enum UrlRegexGroup {
  // 监听器（网关）名称
  ListenerName = 1,
  // composite插件下发的被包装的插件名称，包含插件实例ID
  FilterName,
  // 最终的入口，即HasEpochEntry、NoEpochPEntry二选一的结果
  FinalEntry,
  // 包含Epoch的路径
  HasEpochEntry,
  // Epoch id
  Epoch,
  // 插件controller的路径
  ControllerPath,
  // 不包含Epoch的路径，实际就是插件controller的路径
  NoEpochPEntry
};

SrhinoPluginHandler::SrhinoPluginHandler(ConfigTracker& config_tracker, Server::Instance& server)
    : HandlerContextBase(server), config_tracker_(config_tracker) {}

Http::Code SrhinoPluginHandler::handlerSrhinoPluginHome(absl::string_view /*url*/,
                                                        Http::ResponseHeaderMap& response_headers,
                                                        Buffer::Instance& response, AdminStream&) {
  ProtobufTypes::MessagePtr dump = dumpSrhinoPlugins();
  std::string json;
  if (dump != nullptr) {
    json = MessageUtil::getJsonStringFromMessageOrError(*dump, true, true);
  } else {
    json = "null";
  }
  response.add(json);
  response_headers.setReferenceContentType(Http::Headers::get().ContentTypeValues.Json);

  return Http::Code::OK;
}

Http::Code SrhinoPluginHandler::handlerSrhinoPluginUnSetHandle(
    absl::string_view url, Http::ResponseHeaderMap& /*response_headers*/,
    Buffer::Instance& response, AdminStream& admin_stream) {
  ProtobufTypes::MessagePtr message = dumpSrhinoPlugins();
  envoy::admin::v3::SrhinoPluginsDump* dump =
      dynamic_cast<envoy::admin::v3::SrhinoPluginsDump*>(message.get());
  if (!dump) {
    return Http::Code::InternalServerError;
  }

  std::string url_string(url.data(), url.length());
  std::string query_params_string;
  auto query_pos = url_string.find("?");
  if (query_pos != std::string::npos) {
    query_params_string = url_string.substr(query_pos);
    url_string = url_string.substr(0, query_pos);
  }

  std::smatch sm;
  if (std::regex_match(url_string, sm, url_regex_)) {
    const std::string listener_name = sm.str(UrlRegexGroup::ListenerName);
    const std::string raw_filter_name = sm.str(UrlRegexGroup::FilterName);

    auto normalize_name = [](absl::string_view name) {
      std::string res(name);
      size_t pos = 0;
      while ((pos = res.find(".so.", pos)) != std::string::npos) {
        res.erase(pos, 3);
      }
      if (res.length() >= 3 && res.substr(res.length() - 3) == ".so") {
        res.erase(res.length() - 3);
      }
      return res;
    };

    const std::string norm_filter_name = normalize_name(raw_filter_name);
    std::string actual_installed_name;

    for (auto& plugin : dump->installed_plugins().plugins()) {
      std::string norm_installed = normalize_name(plugin.name());
      if (norm_filter_name.starts_with(norm_installed) &&
          (norm_filter_name.length() == norm_installed.length() || norm_filter_name[norm_installed.length()] == '.')) {
        actual_installed_name = plugin.name(); // 记录原名
        break;
      }
    }

    std::string epoch = sm.str(UrlRegexGroup::Epoch);
    std::string controller_path = sm.str(UrlRegexGroup::ControllerPath);
    std::string actual_binded_name;
    const std::string target_bind_prefix_norm = std::format("{}/{}/", listener_name, norm_filter_name);

    if (epoch.empty()) {
      std::vector<uint64_t> epochs;
      for (auto& plugin : dump->binded_plugins().plugins()) {
        std::string norm_binded = normalize_name(plugin.name());
        if (norm_binded.starts_with(target_bind_prefix_norm)) {
          auto pos = plugin.name().rfind("/");
          if (pos != std::string::npos) {
            epochs.emplace_back(std::atoll(plugin.name().substr(pos + 1).c_str()));
          }
        }
      }

      if (!epochs.empty()) {
        std::sort(epochs.begin(), epochs.end(), std::greater<uint64_t>());
        epoch = std::to_string(epochs.front());
        controller_path = sm.str(UrlRegexGroup::NoEpochPEntry);
      }
    }

    if (!epoch.empty()) {
      std::string exact_bind_norm = target_bind_prefix_norm + epoch;
      for (auto& plugin : dump->binded_plugins().plugins()) {
        if (normalize_name(plugin.name()) == exact_bind_norm) {
          actual_binded_name = plugin.name();
          break;
        }
      }
    }

    bool is_controller_path = false;
    bool is_global = false;
    const std::string json_field_path = std::format(R"("path": "/{}")", controller_path);
    if (!actual_installed_name.empty()) {
      for (auto& plugin : dump->installed_plugins().plugins()) {
        if (plugin.name() == actual_installed_name) {
          for (auto& controller : plugin.controllers()) {
            std::string json = MessageUtil::getJsonStringFromMessageOrError(controller, true, true);
            if (json.find(json_field_path) != std::string::npos) {
              is_controller_path = true;
              if (json.find(R"("global": true)") != std::string::npos) {
                is_global = true;
              }
              break;
            }
          }
          break;
        }
      }
    }

    if (is_controller_path && !actual_binded_name.empty()) {
      for (auto& plugin : dump->binded_plugins().plugins()) {
        if (plugin.name() == actual_binded_name) {
          auto plugin_file = ThreadSafeSingleton<SrhinoPluginFramework::PluginFileManager>::get().getPluginFile(actual_installed_name);
          
          if (plugin_file) {
            const auto method = admin_stream.getRequestHeaders().getMethodValue();
            auto body = admin_stream.getRequestBody();
            std::string pass_url = std::format("/{}{}", controller_path, query_params_string);
            const SrhinoPluginFramework::v1_0_x::CallResult* call_result = nullptr;
            if (body) {
              std::string body_string = body->toString();
              call_result = reinterpret_cast<const SrhinoPluginFramework::v1_0_x::CallResult*>(
                  plugin_file->onControl(method.data(), method.size(), pass_url.data(),
                                         pass_url.size(), body_string.data(), body_string.size(),
                                         is_global ? reinterpret_cast<void*>(plugin.config_ptr())
                                                   : nullptr));
            } else {
              call_result = reinterpret_cast<const SrhinoPluginFramework::v1_0_x::CallResult*>(
                  plugin_file->onControl(
                      method.data(), method.size(), pass_url.data(), pass_url.size(), nullptr, 0,
                      is_global ? reinterpret_cast<void*>(plugin.config_ptr()) : nullptr));
            }

            if (call_result) {
              response.add(call_result->data, call_result->data_size);
            }
            return Http::Code::OK;
          }
        }
      }
    }
  }

  response.add("404\n");
  return Http::Code::NotFound;
}

std::string_view SrhinoPluginHandler::basePath() const { return "/srhino_plugin/"; }

ProtobufTypes::MessagePtr SrhinoPluginHandler::dumpSrhinoPlugins() const {
  static constexpr char srhino_plugins[] = "srhino_plugins";

  const auto& callbacks_map = config_tracker_.getCallbacksMap();
  auto iter = callbacks_map.find(srhino_plugins);
  if (iter != callbacks_map.end()) {
    auto name_matcher = std::make_unique<Matchers::UniversalStringMatcher>();
    return iter->second(*name_matcher);
  }

  return nullptr;
}

} // namespace Server
} // namespace Envoy
