#pragma once

#include "envoy/srhino_plugin_framework/v1_3_x/virtual_host_info.h"

#include "envoy/http/filter.h"
#include "source/common/srhino_plugin_framework/v1_3_x/route_info_impl.h"
#include "source/common/srhino_plugin_framework/config_impl.h"
#include "source/common/router/config_impl.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
class VirtualHostInfoImpl : public VirtualHostInfo {
public:
  VirtualHostInfoImpl(const std::string& filter_name, Envoy::Http::StreamFilterCallbacks* callbacks,
                      const Envoy::Server::Configuration::FactoryContext& factory_context);

public:
  const std::string& name() const override;

public:
  void* config() const override;

public:
  const Envoy::Router::VirtualHostImpl* raw() const { return virtual_host_impl_; }

private:
  void initVirtualHostImpl();

private:
  const std::string& filter_name_;
  Envoy::Http::StreamFilterCallbacks* callbacks_;
  const Envoy::Server::Configuration::FactoryContext& factory_context_;
  const Envoy::Router::VirtualHostImpl* virtual_host_impl_{nullptr};
  std::string name_;
};
} // namespace v1_3_x
} // namespace SrhinoPluginFramework