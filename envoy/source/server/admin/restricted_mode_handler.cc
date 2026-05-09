#include "source/server/admin/restricted_mode_handler.h"

#include <atomic>
#include <fstream>
#include <mutex>

#include "envoy/admin/v3/server_info.pb.h"
#include "envoy/filesystem/filesystem.h"

#include "source/common/http/headers.h"
#include "source/common/http/utility.h"
#include "source/server/admin/utils.h"

namespace Envoy {
namespace Server {

std::atomic<bool> RestrictedModeHandler::restricted_mode_{false};
const std::string RestrictedModeHandler::env_file_path_ = "/opt/public/.env";
const std::string RestrictedModeHandler::env_var_ = "SR_RESTRICTED_MODE=";

RestrictedModeHandler::RestrictedModeHandler(Server::Instance& server)
    : HandlerContextBase(server) {}

bool RestrictedModeHandler::loadStateFromConfigFile() {
  try {
    std::ifstream file(env_file_path_);
    if (!file.is_open()) {
      ENVOY_LOG(warn, "Config file not found: {}, using default: unrestricted", env_file_path_);
      return false;
    }

    std::string line;
    while (std::getline(file, line)) {
      absl::string_view stripped_line = absl::StripAsciiWhitespace(line);
      
      if (stripped_line.empty() || stripped_line[0] == '#') {
        continue;
      }

      if (absl::StartsWith(stripped_line, env_var_)) {
        absl::string_view value = stripped_line.substr(env_var_.length());
        value = absl::StripAsciiWhitespace(value);
        
        std::string value_str(value);
        std::string lower_value = absl::AsciiStrToLower(value_str);
        
        ENVOY_LOG(debug, "Found SR_RESTRICTED_MODE={}", value_str);
        
        if (lower_value == "true") {
          ENVOY_LOG(error, "enter restricted mode");
          return true;
        } else {
          ENVOY_LOG(debug, "SR_RESTRICTED_MODE is not 'true', using unrestricted mode");
          return false;
        }
      }
    }

    ENVOY_LOG(debug, "SR_RESTRICTED_MODE variable not found in config file, using default: unrestricted");
    return false;

  } catch (const std::exception& e) {
    ENVOY_LOG(error, "Exception while reading config file {}: {}", env_file_path_, e.what());
    return false;
  }
}

bool RestrictedModeHandler::isRestrictedMode() {
  static std::once_flag init_flag;
  std::call_once(init_flag, []() {
    bool initial_mode = loadStateFromConfigFile();
    restricted_mode_.store(initial_mode, std::memory_order_release);
    ENVOY_LOG(info, "Restricted mode initialized: {}", initial_mode ? "restricted" : "unrestricted");
  });
  
  return restricted_mode_.load(std::memory_order_acquire);
}

Http::Code RestrictedModeHandler::entry(absl::string_view, Http::ResponseHeaderMap&,
                                        Buffer::Instance& response, AdminStream&) {
  restricted_mode_.store(true, std::memory_order_release);
  response.add("OK\n");
  ENVOY_LOG(error, "enter restricted mode");
  return Http::Code::OK;
}

Http::Code RestrictedModeHandler::exit(absl::string_view, Http::ResponseHeaderMap&,
                                       Buffer::Instance& response, AdminStream&) {
  restricted_mode_.store(false, std::memory_order_release);
  response.add("OK\n");
  ENVOY_LOG(error, "exit restricted mode");
  return Http::Code::OK;
}

Http::Code RestrictedModeHandler::status(absl::string_view,
                                         Http::ResponseHeaderMap& response_headers,
                                         Buffer::Instance& response, AdminStream&) {
  response_headers.setReferenceContentType(Http::Headers::get().ContentTypeValues.Json);
  if (isRestrictedMode()) {
    response.add("{\"status\":\"restricted\"}\n");
  } else {
    response.add("{\"status\":\"unrestricted\"}\n");
  }
  return Http::Code::OK;
}

} // namespace Server
} // namespace Envoy