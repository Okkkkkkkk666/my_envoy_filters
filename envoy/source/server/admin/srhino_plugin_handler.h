#pragma once

#include <regex>

#include "source/server/admin/handler_ctx.h"

namespace Envoy {
namespace Server {

class SrhinoPluginHandler : public HandlerContextBase {

public:
  SrhinoPluginHandler(ConfigTracker& config_tracker, Server::Instance& server);

  Http::Code handlerSrhinoPluginHome(absl::string_view path_and_query,
                                     Http::ResponseHeaderMap& response_headers,
                                     Buffer::Instance& response, AdminStream&);

  Http::Code handlerSrhinoPluginUnSetHandle(absl::string_view path_and_query,
                                            Http::ResponseHeaderMap& response_headers,
                                            Buffer::Instance& response, AdminStream&);

public:
  std::string_view basePath() const;

private:
  ProtobufTypes::MessagePtr dumpSrhinoPlugins() const;

private:
  static const std::regex url_regex_;
  ConfigTracker& config_tracker_;
};

} // namespace Server
} // namespace Envoy
