#include <string>
#include <memory>

#include "config.h"

#include "envoy/registry/registry.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WafFilter {

Http::FilterFactoryCb WafFilterFactory::createFilterFactoryFromProtoTyped(
    const v3::WafGlobal& proto_config, const std::string& stats_prefix,
    Server::Configuration::FactoryContext& context) {

  FilterGlobalConfigSharedPtr config =
      std::make_shared<FilterGlobalConfig>(proto_config, stats_prefix, context);

  return [config, &server_context = context.getServerFactoryContext()](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<Filter>(config, server_context));
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
WafFilterFactory::createRouteSpecificFilterConfigTyped(const v3::WafRoute& proto_config,
                                                       Server::Configuration::ServerFactoryContext&,
                                                       ProtobufMessage::ValidationVisitor&) {
  return std::make_shared<const FilterRouteConfig>(proto_config);
}

REGISTER_FACTORY(WafFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){"envoy.waf"};

} // namespace WafFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy