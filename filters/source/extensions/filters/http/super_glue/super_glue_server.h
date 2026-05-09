#pragma once

#include "source/common/http/headers.h"
#include "source/common/http/message_impl.h"
#include "source/extensions/filters/http/common/pass_through_filter_ex.h"

#include "filters/api/envoy/extensions/filters/http/super_glue/v3/super_glue_server.pb.h"

#include "common.h"

#define FILTER_NAME "envoy.filters.http.super-glue-server.1.0"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SuperGlueFilter {

namespace v3 = envoy::extensions::filters::http::super_glue::v3;

// 全局配置
class ServerFilterGlobalConfig : public Router::RouteSpecificFilterConfig {
public:
  ServerFilterGlobalConfig(const v3::SuperGlueServerGlobal& proto_config,
                           Event::Dispatcher& dispatcher, Stats::Scope& must_be_server_scope);
};

using ServerFilterGlobalConfigSharedPtr = std::shared_ptr<ServerFilterGlobalConfig>;

// VH配置
class ServerFilterRouteConfig : public Router::RouteSpecificFilterConfig {
public:
  ServerFilterRouteConfig(const v3::SuperGlueServerRoute& proto_config,
                          Event::Dispatcher& dispatcher);
};

using ServerFilterRouteConfigSharedPtr = std::shared_ptr<ServerFilterRouteConfig>;

class ServerFilter : public Http::PassThroughFilterEx {
public:
  ServerFilter(ServerFilterGlobalConfigSharedPtr config,
               const Server::Configuration::ServerFactoryContext& context)
      : Http::PassThroughFilterEx(context), filter_hcm_config_(config) {}

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance& data, bool end_stream) override;

  void onStreamComplete() override;
  void sendLocalReply(Http::Code status, const std::string& body = "");

private:
  static const Http::LowerCaseString header_flag_;
  ServerFilterGlobalConfigSharedPtr filter_hcm_config_;
  Http::RequestHeaderMapPtr after_request_header_map_;
};
} // namespace SuperGlueFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
