#include "source/common/srhino_plugin_framework/v1_4_x/route_info_impl.h"

#include "source/common/common/empty_string.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
RouteInfoImpl::RouteInfoImpl(const std::string& filter_name,
                             Envoy::Http::StreamFilterCallbacks* callbacks)
    : filter_name_(filter_name), callbacks_(callbacks) {
  initRouteEntryImpl();
}

const std::string& RouteInfoImpl::name() const { return route_entry_impl_->routeName(); }

const std::string& RouteInfoImpl::filterName() const { return filter_name_; }

void* RouteInfoImpl::config() const {
  if (!route_entry_impl_) {
    return nullptr;
  }
  auto envoy_config = route_entry_impl_->perFilterConfig(filter_name_);
  if (!envoy_config) {
    return nullptr;
  }
  auto srhino_per_route_config =
      dynamic_cast<const Envoy::Router::SrhinoRouteSpecificFilterConfig*>(envoy_config);
  if (!srhino_per_route_config) {
    return nullptr;
  }
  return srhino_per_route_config->runtimeConfig();
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

} // namespace v1_4_x
} // namespace SrhinoPluginFramework