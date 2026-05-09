#include "envoy/registry/registry.h"

#include "server_config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SuperGlueFilter {

Http::FilterFactoryCb SuperGlueServerFilterFactory::createFilterFactoryFromProtoTyped(
    const v3::SuperGlueServerGlobal& proto_config, const std::string&,
    Server::Configuration::FactoryContext& context) {
  auto filter_config = std::make_shared<ServerFilterGlobalConfig>(
      proto_config, context.mainThreadDispatcher(), context.serverScope());

  return [filter_config, &server_context = context.getServerFactoryContext()](
             Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<ServerFilter>(filter_config, server_context));
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
SuperGlueServerFilterFactory::createRouteSpecificFilterConfigTyped(
    const v3::SuperGlueServerRoute& proto_config,
    Server::Configuration::ServerFactoryContext& context, ProtobufMessage::ValidationVisitor&) {
  return std::make_shared<ServerFilterRouteConfig>(proto_config, context.mainThreadDispatcher());
}

REGISTER_FACTORY(SuperGlueServerFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){"envoy.super_glue.server"};

} // namespace SuperGlueFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
