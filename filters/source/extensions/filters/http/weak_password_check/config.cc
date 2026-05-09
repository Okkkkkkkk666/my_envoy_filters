#include <string>
#include "envoy/registry/registry.h"
#include "config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WeakPasswordCheck {

Http::FilterFactoryCb WeakPasswordCheckFilterFactory::createFilterFactoryFromProtoTyped(
    const v3::WeakPasswordCheckGlobal& proto_config, const std::string&,
    Server::Configuration::FactoryContext& context) {
  auto filter_config = std::make_shared<FilterGlobalConfig>(proto_config, context);

  return [filter_config, &server_context = context.getServerFactoryContext()](
             Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<Filter>(filter_config, server_context));
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
WeakPasswordCheckFilterFactory::createRouteSpecificFilterConfigTyped(
    const v3::WeakPasswordCheckRoute& proto_config,
    Server::Configuration::ServerFactoryContext& context, ProtobufMessage::ValidationVisitor&) {
  return std::make_shared<FilterRouteConfig>(proto_config, context, context.mainThreadDispatcher());
}

REGISTER_FACTORY(WeakPasswordCheckFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){"envoy.weak_password_check"};
} // namespace WeakPasswordCheck
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
