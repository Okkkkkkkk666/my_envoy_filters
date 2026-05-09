#pragma once

#include "envoy/srhino_plugin_framework/v1_4_x/route_info.h"

#include "envoy/http/filter.h"
#include "source/common/srhino_plugin_framework/config_impl.h"
#include "source/common/router/config_impl.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
class RouteInfoImpl : public RouteInfo {
public:
  RouteInfoImpl(const std::string& filter_name, Envoy::Http::StreamFilterCallbacks* callbacks);

public:
  const std::string& name() const override;
  const std::string& filterName() const override;

public:
  void* config() const override;

public:
  const Envoy::Router::RouteEntryImplBase* raw() const { return route_entry_impl_; }

private:
  void initRouteEntryImpl();

private:
  const std::string& filter_name_;
  Envoy::Http::StreamFilterCallbacks* callbacks_;
  const Envoy::Router::RouteEntryImplBase* route_entry_impl_{nullptr};
};
} // namespace v1_4_x
} // namespace SrhinoPluginFramework