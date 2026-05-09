#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "envoy/registry/registry.h"

#include "config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {

Http::FilterFactoryCb StrongStatefulSessionFilterFactory::createFilterFactoryFromProtoTyped(
    const v3::StrongStatefulSessionGlobal& proto_config, const std::string&,
    Server::Configuration::FactoryContext& context) {
  auto filter_config = std::make_shared<FilterGlobalConfig>(proto_config,context);

  return
      [filter_config, proto_config, &context, &server_context = context.getServerFactoryContext()](
          Http::FilterChainFactoryCallbacks& callbacks) -> void {
        if (proto_config.has_grpc_service()) {
          const std::chrono::milliseconds timeout = std::chrono::milliseconds(
              PROTOBUF_GET_MS_OR_DEFAULT(proto_config.grpc_service(), timeout, 20));
          auto client = std::make_unique<Filters::Common::CentralDatabase::Database>(
              context, proto_config.grpc_service(), timeout, FILTER_NAME);
          callbacks.addStreamFilter(
              std::make_shared<Filter>(filter_config, server_context, std::move(client)));
        } else {
          callbacks.addStreamFilter(std::make_shared<Filter>(
              filter_config, server_context, Filters::Common::CentralDatabase::DatabasePtr()));
        }
      };
}

Router::RouteSpecificFilterConfigConstSharedPtr 
StrongStatefulSessionFilterFactory::createRouteSpecificFilterConfigTyped(
  const v3::StrongStatefulSessionPerRoute&
   proto_config,
   Server::Configuration::ServerFactoryContext&, ProtobufMessage::ValidationVisitor&){
  return std::make_shared<const FilterRouteConfig>(proto_config);
}

REGISTER_FACTORY(StrongStatefulSessionFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){
    "envoy.strong_stateful_session"};

} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
