#include "source/common/srhino_plugin_framework/v1_2_x/route_info_impl.h"

#include "source/common/common/empty_string.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
RouteInfoImpl::RouteInfoImpl(const std::string& filter_name,
                             Envoy::Http::StreamFilterCallbacks* callbacks)
    : filter_name_(filter_name), compatible_filter_instance_name_("compatible_" + filter_name),
      callbacks_(callbacks) {
  initRouteEntryImpl();
}

const std::string& RouteInfoImpl::name() const { return route_entry_impl_->routeName(); }

ConfigPerRouteInterface* RouteInfoImpl::configInternal() const {
  auto config = perFilterConfig();
  if (config) {
    return config->config().get();
  }

  return nullptr;
}

std::string RouteInfoImpl::configJsonString() const {
  auto config = perFilterConfig();
  if (config) {
    return config->configJsonString();
  }

  return Envoy::EMPTY_STRING;
}

void RouteInfoImpl::setConfig(ConfigPerRouteInterfaceSharedPtr config) const {
  auto config_impl = perFilterConfig();
  if (config_impl) {
    return config_impl->setConfig(config);
  }
}

void RouteInfoImpl::initRouteEntryImpl() {
  auto route = callbacks_->route();
  if (route) {
    auto entry = route->routeEntry();
    if (entry) {
      route_entry_impl_ = dynamic_cast<const Envoy::Router::RouteEntryImplBase*>(entry);
    } else {
      auto route_impl = std::dynamic_pointer_cast<const Envoy::Router::RouteEntryImplBase>(route);
      if (route_impl && route_impl->isDirectResponse()) {
        route_entry_impl_ = route_impl.get();
      }
    }
  }
}

ConfigPerRouteImpl* RouteInfoImpl::perFilterConfig() const {
  if (route_entry_impl_) {
    auto envoy_config = route_entry_impl_->perFilterConfig(compatible_filter_instance_name_);
    if (envoy_config) {
      auto srhino_config = dynamic_cast<const ConfigPerRouteImpl*>(envoy_config);
      return const_cast<ConfigPerRouteImpl*>(srhino_config);
    }
  }

  return nullptr;
}

} // namespace v1_2_x
} // namespace SrhinoPluginFramework