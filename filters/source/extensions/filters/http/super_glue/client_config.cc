#include <string>
#include "envoy/registry/registry.h"
#include "client_config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SuperGlueFilter {

Http::FilterFactoryCb SuperGlueClientFilterFactory::createFilterFactoryFromProtoTyped(
    const v3::SuperGlueClientGlobal& proto_config, const std::string&,
    Server::Configuration::FactoryContext& context) {
  auto filter_config = std::make_shared<ClientFilterGlobalConfig>(proto_config, context);

  return [filter_config, &server_context = context.getServerFactoryContext()](
             Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<ClientFilter>(filter_config, server_context));
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
SuperGlueClientFilterFactory::createRouteSpecificFilterConfigTyped(
    const v3::SuperGlueClientRoute& proto_config,
    Server::Configuration::ServerFactoryContext& context, ProtobufMessage::ValidationVisitor&) {
  return std::make_shared<ClientFilterRouteConfig>(proto_config, context);
}

REGISTER_FACTORY(SuperGlueClientFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){"envoy.super_glue.client"};

} // namespace SuperGlueFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
