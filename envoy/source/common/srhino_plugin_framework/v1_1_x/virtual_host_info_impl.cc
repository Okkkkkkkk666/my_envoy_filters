#include "source/common/srhino_plugin_framework/v1_1_x/virtual_host_info_impl.h"
#include "source/common/common/empty_string.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
VirtualHostInfoImpl::VirtualHostInfoImpl(
    const std::string& filter_name, Envoy::Http::StreamFilterCallbacks* callbacks,
    const Envoy::Server::Configuration::FactoryContext& factory_context)
    : filter_name_(filter_name), compatible_filter_instance_name_("compatible_" + filter_name), callbacks_(callbacks), factory_context_(factory_context) {
  initVirtualHostImpl();
}

const std::string& VirtualHostInfoImpl::name() const {
  if (name_.empty() && virtual_host_impl_) {
    const_cast<std::string&>(name_) =
        factory_context_.getServerFactoryContext().scope().symbolTable().toString(
            virtual_host_impl_->statName());
  }

  return name_;
}

ConfigPerRouteInterface* VirtualHostInfoImpl::configInternal() const {
  auto config = perFilterConfig();
  if (config) {
    return config->config().get();
  }

  return nullptr;
}

std::string VirtualHostInfoImpl::configJsonString() const {
  auto config = perFilterConfig();
  if (config) {
    return config->configJsonString();
  }

  return Envoy::EMPTY_STRING;
}

void VirtualHostInfoImpl::setConfig(ConfigPerRouteInterfaceSharedPtr config) const {
  auto config_impl = perFilterConfig();
  if (config_impl) {
    return config_impl->setConfig(config);
  }
}

void VirtualHostInfoImpl::initVirtualHostImpl() {
  auto route = callbacks_->route();
  if (route) {
    auto entry = route->routeEntry();
    if (entry) {
      auto& vh = entry->virtualHost();
      virtual_host_impl_ = dynamic_cast<const Envoy::Router::VirtualHostImpl*>(&vh);
    } else {
      auto route_impl = std::dynamic_pointer_cast<const Envoy::Router::RouteEntryImplBase>(route);
      if (route_impl && route_impl->isDirectResponse()) {
        const Envoy::Router::VirtualHost& vh = route_impl->virtualHost();
        virtual_host_impl_ = dynamic_cast<const Envoy::Router::VirtualHostImpl*>(&vh);
      }
    }
  }
}

ConfigPerRouteImpl* VirtualHostInfoImpl::perFilterConfig() const {
  if (virtual_host_impl_) {
    auto envoy_config = virtual_host_impl_->perFilterConfig(compatible_filter_instance_name_);
    auto srhino_config = dynamic_cast<const ConfigPerRouteImpl*>(envoy_config);
    return const_cast<ConfigPerRouteImpl*>(srhino_config);
  }

  return nullptr;
}
} // namespace v1_1_x
} // namespace SrhinoPluginFramework