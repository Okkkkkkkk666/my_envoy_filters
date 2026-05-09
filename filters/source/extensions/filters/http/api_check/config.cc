#include <string>
#include <memory>

#include "config.h"

#include "envoy/registry/registry.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ApiCheckFilter {

Http::FilterFactoryCb ApiCheckFilterFactory::createFilterFactoryFromProtoTyped(
    const v3::ApiCheckGlobal& proto_config, const std::string& stats_prefix,
    Server::Configuration::FactoryContext& context) {

  FilterGlobalConfigSharedPtr config =
      std::make_shared<FilterGlobalConfig>(proto_config, stats_prefix, context);

  return [config, &server_context = context.getServerFactoryContext()](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<Filter>(config, server_context));
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
ApiCheckFilterFactory::createRouteSpecificFilterConfigTyped(const v3::ApiCheckRoute& proto_config,
                                                       Server::Configuration::ServerFactoryContext&,
                                                       ProtobufMessage::ValidationVisitor&) {
  return std::make_shared<const FilterRouteConfig>(proto_config);
}

REGISTER_FACTORY(ApiCheckFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){"envoy.api_check"};

} // namespace ApiCheckFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy