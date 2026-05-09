#pragma once

#include "envoy/srhino_plugin_framework/v1_0_x/route_info.h"

#include "envoy/http/filter.h"
#include "source/common/srhino_plugin_framework/config_impl.h"
#include "source/common/router/config_impl.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
class RouteInfoImpl : public RouteInfo {
public:
  RouteInfoImpl(const std::string& filter_name, Envoy::Http::StreamFilterCallbacks* callbacks);

public:
  const std::string& name() const override;

protected:
  ConfigPerRouteInterface* configInternal() const override;
  std::string configJsonString() const override;
  void setConfig(ConfigPerRouteInterfaceSharedPtr config) const override;

public:
  const Envoy::Router::RouteEntryImplBase* raw() const { return route_entry_impl_; }

private:
  void initRouteEntryImpl();
  ConfigPerRouteImpl* perFilterConfig() const;

private:
  const std::string& filter_name_;
  const std::string compatible_filter_instance_name_;
  Envoy::Http::StreamFilterCallbacks* callbacks_;
  const Envoy::Router::RouteEntryImplBase* route_entry_impl_{nullptr};
};
} // namespace v1_0_x
} // namespace SrhinoPluginFramework