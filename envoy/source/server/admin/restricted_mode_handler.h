#pragma once

#include "envoy/server/admin.h"
#include "envoy/filesystem/filesystem.h"

#include "source/server/admin/handler_ctx.h"

#include "absl/strings/string_view.h"

namespace Envoy {
namespace Server {

class RestrictedModeHandler : public HandlerContextBase, Logger::Loggable<Logger::Id::admin> {
public:
  RestrictedModeHandler(Server::Instance& server);

  Http::Code entry(absl::string_view path_and_query, Http::ResponseHeaderMap& response_headers,
                   Buffer::Instance& response, AdminStream&);

  Http::Code exit(absl::string_view path_and_query, Http::ResponseHeaderMap& response_headers,
                  Buffer::Instance& response, AdminStream&);

  Http::Code status(absl::string_view path_and_query, Http::ResponseHeaderMap& response_headers,
                    Buffer::Instance& response, AdminStream&);

  static bool isRestrictedMode();

private:
  static std::atomic<bool> restricted_mode_;
  static const std::string env_file_path_;
  static const std::string env_var_;
  static bool loadStateFromConfigFile();
};

} // namespace Server
} // namespace Envoy