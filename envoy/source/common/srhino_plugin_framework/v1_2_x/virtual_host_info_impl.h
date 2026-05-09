#pragma once

#include "envoy/srhino_plugin_framework/v1_2_x/virtual_host_info.h"

#include "envoy/http/filter.h"
#include "source/common/srhino_plugin_framework/v1_2_x/route_info_impl.h"
#include "source/common/srhino_plugin_framework/config_impl.h"
#include "source/common/router/config_impl.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
class VirtualHostInfoImpl : public VirtualHostInfo {
public:
  VirtualHostInfoImpl(const std::string& filter_name, Envoy::Http::StreamFilterCallbacks* callbacks,
                      const Envoy::Server::Configuration::FactoryContext& factory_context);

public:
  const std::string& name() const override;

protected:
  ConfigPerRouteInterface* configInternal() const override;
  std::string configJsonString() const override;
  void setConfig(ConfigPerRouteInterfaceSharedPtr config) const override;

public:
  const Envoy::Router::VirtualHostImpl* raw() const { return virtual_host_impl_; }

private:
  void initVirtualHostImpl();
  ConfigPerRouteImpl* perFilterConfig() const;

private:
  const std::string& filter_name_;
  const std::string compatible_filter_instance_name_;
  Envoy::Http::StreamFilterCallbacks* callbacks_;
  const Envoy::Server::Configuration::FactoryContext& factory_context_;
  const Envoy::Router::VirtualHostImpl* virtual_host_impl_{nullptr};
  std::string name_;
};
} // namespace v1_2_x
} // namespace SrhinoPluginFramework