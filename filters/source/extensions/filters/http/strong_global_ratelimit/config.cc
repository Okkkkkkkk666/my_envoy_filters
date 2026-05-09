#include "config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongGlobalRatelimit {

Http::FilterFactoryCb
Envoy::Extensions::HttpFilters::StrongGlobalRatelimit::StrongGlobalRatelimitFilterFactory::
    createFilterFactoryFromProtoTyped(const v3::StrongGlobalRateLimitGlobal& proto_config,
                                      const std::string&,
                                      Server::Configuration::FactoryContext& context) {
  FilterGlobalConfigSharedPtr filter_config =
      std::make_shared<StrongGlobalRateLimitFilterGlobalConfig>(proto_config);
  const std::chrono::milliseconds timeout =
      std::chrono::milliseconds(PROTOBUF_GET_MS_OR_DEFAULT(proto_config.grpc(), timeout, 20));
  return [proto_config, filter_config, timeout,
          &context](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<StrongGlobalRateLimitFilter>(
        filter_config,
        Filters::Common::RatelimitClient::Impl::rateLimitClient(context, proto_config.grpc(),
                                                                timeout),
        context.getServerFactoryContext()));
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
StrongGlobalRatelimitFilterFactory::createRouteSpecificFilterConfigTyped(
    const v3::StrongGlobalRateLimitRoute& proto_config,
    Server::Configuration::ServerFactoryContext&, ProtobufMessage::ValidationVisitor&) {
  return std::make_shared<StrongGlobalRateLimitFilterRouteConfig>(proto_config);
}

REGISTER_FACTORY(StrongGlobalRatelimitFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){
    "envoy.strong_global_ratelimit"};

} // namespace StrongGlobalRatelimit
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
