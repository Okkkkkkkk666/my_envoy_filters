#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "envoy/registry/registry.h"

#include "config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {

Http::FilterFactoryCb StrongLocalRateLimitFilterFactory::createFilterFactoryFromProtoTyped(
    const v3::StrongLocalRateLimitGlobal& proto_config, const std::string&,
    Server::Configuration::FactoryContext& context) {
  auto filter_config = std::make_shared<FilterGlobalConfig>(
      proto_config, context.mainThreadDispatcher(),
      // context.scope()
      // 注意此处不能用scope，而应该用serverScope，因为scope随着hcm销毁而销毁，serverScope则不会
      context.serverScope());

  return [filter_config, &server_context = context.getServerFactoryContext()](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<Filter>(filter_config,server_context));
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
StrongLocalRateLimitFilterFactory::createRouteSpecificFilterConfigTyped(
    const envoy::extensions::filters::http::strong_local_ratelimit::v3::StrongLocalRateLimitRoute&
        proto_config,
    Server::Configuration::ServerFactoryContext& context, ProtobufMessage::ValidationVisitor&) {
  return std::make_shared<FilterRouteConfig>(proto_config, context.mainThreadDispatcher());
}

/**
 * Static
 * registration
 * for the
 * another
 * local
 * rate
 * limit
 * filter.
 * @see
 * RegisterFactory.
 */
REGISTER_FACTORY(StrongLocalRateLimitFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){"envo"
                                                                      "y."
                                                                      "stro"
                                                                      "ng_"
                                                                      "loca"
                                                                      "l_"
                                                                      "rate"
                                                                      "limi"
                                                                      "t"};

} // namespace
  // StrongLocalRateLimitFilter
} // namespace
  // HttpFilters
} // namespace
  // Extensions
} // namespace
  // Envoy
