#include "filter.h"

#include <format>

#include "envoy/srhino_plugin_framework/v1_0_x/utility/proto_tools.hpp"
#include "source/common/singleton/threadsafe_singleton.h"
#include "source/common/srhino_plugin_framework/plugin_file_manager.h"

#include "envoy/admin/v3/srhino_plugin_dump.pb.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SrhinoPluginLoaderFilter {
Filters::Common::LruCache<std::string, uint64_t> FilterGlobalConfig::filter_epoch_;
const std::string Filter::filter_name_(FILTER_NAME);

FilterGlobalConfig::FilterGlobalConfig(const v3::SrhinoPluginLoaderGlobal& proto_config,
                                       Server::Configuration::FactoryContext& context)
    : proto_config_(proto_config.config()), factory_context_(context) {
  std::string file_name = proto_config.plugin_file_name();
  // 获取插件实例名称
  auto& metadata = context.listenerMetadata().filter_metadata();
  auto iter = metadata.find(HttpFilterNames::get().Composite);
  if (iter != metadata.end()) {
    composite_filter_name_ = iter->second.fields().at("filter_name").string_value();
  } else {
    // 兼容本地调试(不是XDS下发的配置，没有使用ExtensionWithMatcher包装器)
    composite_filter_name_ = file_name;
    auto pos = file_name.find_last_of('.');
    if (pos == std::string::npos) {
      ENVOY_LOG(error, "plugin file name error:{}", file_name);
    } else {
      file_name = file_name.substr(0, pos);
    }
    // composite_filter_name_ = proto_config.plugin_file_name() + ".local_test_id";
  }

  timer_ =
      context.mainThreadDispatcher().createTimer([&, file_name]() { queryPluginFile(file_name); });
  timer_->enableTimer(std::chrono::seconds(1));
}

FilterGlobalConfig::~FilterGlobalConfig() {
  if (timer_) {
    timer_->disableTimer();
  }

  onUnbind();
}

void FilterGlobalConfig::queryPluginFile(const std::string& file_name) {
  ASSERT_IS_MAIN_OR_TEST_THREAD();

  auto plugin_file =
      ThreadSafeSingleton<SrhinoPluginFramework::PluginFileManager>::get().getPluginFile(file_name);
  if (!plugin_file) {
    timer_->enableTimer(std::chrono::seconds(1));
  } else {
    onBind(plugin_file);
  }
}

void FilterGlobalConfig::onBind(SrhinoPluginFramework::PluginFileSharedPtr plugin_file) {
  ASSERT_IS_MAIN_OR_TEST_THREAD();

  if (plugin_file && plugin_file->isOpen()) {
    std::string json;
    std::string error =
        SrhinoPluginFramework::v1_0_x::Utility::ProtoTools::messageToJson(proto_config_, json);
    if (error.empty()) {
      runtime_config_ = plugin_file->onBind(json.c_str(), json.length(), &factory_context_);
      if (runtime_config_) {
        binded_name_ = makeBindedName();
        plugin_file->addBinded(binded_name_, runtime_config_);
        plugin_file_ = plugin_file;
      } else {
        ENVOY_LOG(warn, "Plugin bind failed, config json:{}", json);
      }
    } else {
      ENVOY_LOG(warn, "message to json error:{}", error);
    }
  }
}

void FilterGlobalConfig::onUnbind() {
  ASSERT_IS_MAIN_OR_TEST_THREAD();

  if (plugin_file_ && plugin_file_->isOpen()) {
    plugin_file_->onUnbind(runtime_config_);
    plugin_file_->removeBinded(binded_name_);
  }
}

// name格式：网关名称/插件实例名称/自增标识符
// 因为排水机制,FilterGlobalConfig的析构函数将被延迟执行，这造成同一个name被连续添加两次后，排水结束时将被删除。
// 这对于正在运行的FilterGlobalConfig来说，就丢失了对应name的数据。为了防止这种情况的发生，需要保证key不重复，所以在name
// 中引入自增标识符
std::string FilterGlobalConfig::makeBindedName() {
  uint64_t epoch = 0;
  filter_epoch_.access(
      composite_filter_name_,
      [&](uint64_t& filter_epoch) {
        epoch = filter_epoch;
        ++filter_epoch;
      },
      [&]() { return epoch; });

  return std::format("{}/{}/{}", factory_context_.listenerName(), composite_filter_name_, epoch);
}

FilterRouteConfig::FilterRouteConfig(const v3::SrhinoPluginLoaderPerRoute& proto_config,
                                     Envoy::Server::Configuration::ServerFactoryContext& context)
    : proto_config_(proto_config.config()), factory_context_(context) {
  std::string file_name = proto_config.plugin_file_name();
  timer_ =
      context.mainThreadDispatcher().createTimer([&, file_name]() { queryPluginFile(file_name); });
  timer_->enableTimer(std::chrono::seconds(1));
}

FilterRouteConfig::~FilterRouteConfig() {
  if (timer_) {
    timer_->disableTimer();
  }
  onUnbind();
}

void FilterRouteConfig::queryPluginFile(const std::string& file_name) {
  ASSERT_IS_MAIN_OR_TEST_THREAD();
  auto plugin_file = ThreadSafeSingleton<SrhinoPluginFramework::PluginFileManager>::get().getPluginFile(file_name);
  if (!plugin_file) {
    timer_->enableTimer(std::chrono::seconds(1));
  } else {
    onBind(plugin_file);
  }
}

void FilterRouteConfig::onBind(SrhinoPluginFramework::PluginFileSharedPtr plugin_file) {
  ASSERT_IS_MAIN_OR_TEST_THREAD();

  if (plugin_file && plugin_file->isOpen()) {
    std::string json;
    std::string error =
        SrhinoPluginFramework::v1_0_x::Utility::ProtoTools::messageToJson(proto_config_, json);
    if (error.empty()) {
      void *rt_cfg = plugin_file->onBindRoute(json.c_str(), json.length());
      if (rt_cfg) {
        ENVOY_LOG(info, "plugin: {}, create route success", plugin_file->pluginFileInfo().file_name_);
        plugin_file_ = plugin_file;
        std::lock_guard<std::mutex> lock(runtime_config_mtx_);
        runtime_config_ = rt_cfg;
      } else {
        ENVOY_LOG(error, "plugin: {}, create route failed", plugin_file->pluginFileInfo().file_name_);
      }
    }
  }
}

void FilterRouteConfig::onUnbind() {
  ASSERT_IS_MAIN_OR_TEST_THREAD();
  if (plugin_file_ && plugin_file_->isOpen()) {
    plugin_file_->onUnbindRoute(runtime_config_);
  }
}


Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers, bool end_stream) {
  ENVOY_LOG(trace, ">>>>>>decodeHeaders: call onHeader");
  if (!plugin_instance_) {
    return Http::FilterHeadersStatus::Continue;
  }
  is_normal_stream_ = true;

  return onHeader(decode_status_, decoder_callbacks_, nullptr, headers, end_stream);
}

Http::FilterDataStatus Filter::decodeData(Buffer::Instance& data, bool end_stream) {
  ENVOY_LOG(trace, ">>>>>>decodeData: call onData");
  if (!plugin_instance_) {
    return Http::FilterDataStatus::Continue;
  }

  return onData(decode_status_, decoder_callbacks_, nullptr, is_buffer_full_, data, end_stream);
}

Http::FilterTrailersStatus Filter::decodeTrailers(Http::RequestTrailerMap& trailers) {
  ENVOY_LOG(trace, ">>>>>>decodeTrailers: call onTrailer");
  if (!plugin_instance_) {
    return Http::FilterTrailersStatus::Continue;
  }

  return onTrailer(decode_status_, decoder_callbacks_, nullptr, trailers);
}

Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) {
  ENVOY_LOG(trace, "encodeHeaders<<<<<<: call onHeader");
  if (!plugin_instance_ || !is_normal_stream_ || encoder_callbacks_->streamInfo().isLocalReply()) {
    return Http::FilterHeadersStatus::Continue;
  }

  return onHeader(encode_status_, nullptr, encoder_callbacks_, headers, end_stream);
}

Http::FilterDataStatus Filter::encodeData(Buffer::Instance& data, bool end_stream) {
  ENVOY_LOG(trace, "encodeData<<<<<<: call onData");
  if (!plugin_instance_ || !is_normal_stream_ || encoder_callbacks_->streamInfo().isLocalReply()) {
    return Http::FilterDataStatus::Continue;
  }

  return onData(encode_status_, nullptr, encoder_callbacks_, is_buffer_full_, data, end_stream);
}

Http::FilterTrailersStatus Filter::encodeTrailers(Http::ResponseTrailerMap& trailers) {
  ENVOY_LOG(trace, "encodeTrailers<<<<<<: call onTrailer");
  if (!plugin_instance_ || !is_normal_stream_ || encoder_callbacks_->streamInfo().isLocalReply()) {
    return Http::FilterTrailersStatus::Continue;
  }

  return onTrailer(encode_status_, nullptr, encoder_callbacks_, trailers);
}

void Filter::onStreamComplete() {
  if (!plugin_instance_) {
    return;
  }

  // 如果是本地回复，并且upstreamInfo或upstreamHost为空
  if (encoder_callbacks_->streamInfo().isLocalReply()) {
    auto upstream_info = encoder_callbacks_->streamInfo().upstreamInfo();
    if (!upstream_info || !upstream_info->upstreamHost()) {
      setPredictUpstreamRemoteAddress();
    }
  }
  ON_STREAM_COMPLETE(plugin_file_->pluginFileInfo().framework_min_required_);
}

Http::FilterHeadersStatus
Filter::onHeader(MultiVersionFilterStatus& filter_status,
                 Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks,
                 Envoy::Http::StreamEncoderFilterCallbacks* encoder_callbacks,
                 Http::HeaderMap& headers, bool end_stream) {
  ON_HEADER(plugin_file_->pluginFileInfo().framework_min_required_, decoder_callbacks,
            encoder_callbacks);
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus Filter::onData(MultiVersionFilterStatus& filter_status,
                                      Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks,
                                      Envoy::Http::StreamEncoderFilterCallbacks* encoder_callbacks,
                                      bool& is_buffer_full, Buffer::Instance& data,
                                      bool end_stream) {
  ON_DATA(plugin_file_->pluginFileInfo().framework_min_required_, decoder_callbacks,
          encoder_callbacks);
  return Http::FilterDataStatus::Continue;
}

Http::FilterTrailersStatus
Filter::onTrailer(MultiVersionFilterStatus& filter_status,
                  Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks,
                  Envoy::Http::StreamEncoderFilterCallbacks* encoder_callbacks,
                  Http::HeaderMap& trailers) {
  ON_TRAILER(plugin_file_->pluginFileInfo().framework_min_required_, decoder_callbacks,
             encoder_callbacks);
  return Http::FilterTrailersStatus::Continue;
}

void Filter::adjustBufferLimit(Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks,
                               const Buffer::Instance& data) {
  ASSERT(decoder_callbacks);

  uint64_t total_length = data.length();
  if (decoder_callbacks->decodingBuffer() && decoder_callbacks->decodingBuffer() != &data) {
    total_length += decoder_callbacks->decodingBuffer()->length();
  }
  if (total_length > decoder_callbacks->decoderBufferLimit()) {
    decoder_callbacks->setDecoderBufferLimit(decoder_callbacks->decoderBufferLimit() +
                                             data.length());
  }
}

void Filter::adjustBufferLimit(Envoy::Http::StreamEncoderFilterCallbacks* encoder_callbacks,
                               const Buffer::Instance& data) {
  ASSERT(encoder_callbacks);

  uint64_t total_length = data.length();
  if (encoder_callbacks->encodingBuffer() && encoder_callbacks->encodingBuffer() != &data) {
    total_length += encoder_callbacks->encodingBuffer()->length();
  }
  if (total_length > encoder_callbacks->encoderBufferLimit()) {
    encoder_callbacks->setEncoderBufferLimit(encoder_callbacks->encoderBufferLimit() +
                                             data.length());
  }
}


inline bool Filter::setPredictUpstreamRemoteAddress() const {
  if (!decoder_callbacks_) {
    return false;
  }
  if (decoder_callbacks_->streamInfo().predictUpstreamRemoteAddress()) {
    return true;
  }

  Upstream::ClusterInfoConstSharedPtr cluster_info =
      decoder_callbacks_->streamInfo().upstreamClusterInfo().has_value()
          ? decoder_callbacks_->streamInfo().upstreamClusterInfo().value()
          : nullptr;
  if (!cluster_info) {
    return false;
  }

  auto cluster = const_cast<Server::Configuration::FactoryContext&>(factory_context_)
                     .clusterManager()
                     .getThreadLocalCluster(cluster_info->name());
  if (cluster == nullptr) {
    return false;
  }

  const auto& host = cluster->loadBalancer().peekAnotherHost(nullptr);
  if (host) {
    ENVOY_LOG(trace, "peekAnotherHost:{}", host->address()->asString());
    filter_hcm_config_->setPredictUpstreamRemoteAddress(host->address());
    decoder_callbacks_->streamInfo().setPredictUpstreamRemoteAddress(host->address());
  } else {
    if (filter_hcm_config_->predictUpstreamRemoteAddress()) {
      ENVOY_LOG(trace, "filter_hcm_config_->predictUpstreamRemoteAddress():{}",
                filter_hcm_config_->predictUpstreamRemoteAddress()->asString());
      decoder_callbacks_->streamInfo().setPredictUpstreamRemoteAddress(
          filter_hcm_config_->predictUpstreamRemoteAddress());
    } else {
      const auto& host_sets = cluster->prioritySet().hostSetsPerPriority();
      if (host_sets.empty()) {
        ENVOY_LOG(trace, "host_sets is empty");
        return false;
      } else {
        // 遍历所有优先级，寻找健康主机
        bool flag = false;
        for (const auto& host_set : host_sets) {
          const auto& healthy_hosts = host_set->healthyHosts();
          if (!healthy_hosts.empty()) {
            const auto& host = healthy_hosts[0];
            filter_hcm_config_->setPredictUpstreamRemoteAddress(host->address());
            decoder_callbacks_->streamInfo().setPredictUpstreamRemoteAddress(host->address());
            ENVOY_LOG(trace, "Using first healthy host from priority {}: {}", host_set->priority(),
                      host->address()->asString());
            flag = true;
            break;
          }
        }

        // 如果所有优先级都没有健康主机，尝试使用任何可用的主机
        if (!flag) {
          for (const auto& host_set : host_sets) {
            const auto& hosts = host_set->hosts();
            if (!hosts.empty()) {
              const auto& host = hosts[0];
              filter_hcm_config_->setPredictUpstreamRemoteAddress(host->address());
              decoder_callbacks_->streamInfo().setPredictUpstreamRemoteAddress(host->address());
              ENVOY_LOG(trace, "No healthy hosts, using first available host from priority {}: {}",
                        host_set->priority(), host->address()->asString());
              break;
            }
          }
        }
      }
    }
  }
  return true;
}

} // namespace SrhinoPluginLoaderFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy