#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "envoy/registry/registry.h"

#include "config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SrhinoPluginLoaderFilter {

Http::FilterFactoryCb SrhinoPluginLoaderFilterFactory::createFilterFactoryFromProtoTyped(
    const v3::SrhinoPluginLoaderGlobal& proto_config, const std::string&,
    Server::Configuration::FactoryContext& context) {
  auto filter_config = std::make_shared<FilterGlobalConfig>(proto_config, context);

  return [filter_config, proto_config,
          &context](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<Filter>(filter_config, context));
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
SrhinoPluginLoaderFilterFactory::createRouteSpecificFilterConfigTyped(
    const v3::SrhinoPluginLoaderPerRoute& proto_config,
    Server::Configuration::ServerFactoryContext& context, ProtobufMessage::ValidationVisitor&) {
  auto route_config = std::shared_ptr<FilterRouteConfig>(
      new FilterRouteConfig(proto_config, context), [&context](FilterRouteConfig* ptr) {
        if (Thread::MainThread::isMainOrTestThread()) {
          delete ptr;
        } else {
          context.mainThreadDispatcher().post([ptr]() { delete ptr; });
        }
      });
  return route_config;
}

REGISTER_FACTORY(SrhinoPluginLoaderFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){"envoy.srhino_plugin_loader"};

} // namespace SrhinoPluginLoaderFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
