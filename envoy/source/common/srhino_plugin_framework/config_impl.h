#pragma once

#include "envoy/srhino_plugin_framework/config.h"

#include "source/common/protobuf/protobuf.h"
#include "envoy/router/router.h"

namespace SrhinoPluginFramework {
class ConfigPerRouteImpl : public Envoy::Router::RouteSpecificFilterConfig,
                           public ConfigPerRouteInterface {
public:
  ConfigPerRouteImpl(google::protobuf::Struct&& proto_config);

public:
  ConfigPerRouteInterfaceSharedPtr config() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
  }
  void setConfig(ConfigPerRouteInterfaceSharedPtr config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
  }
  std::string configJsonString() const;
  void lock() { mutex_.lock(); }
  void unlock() { mutex_.unlock(); }

private:
  const google::protobuf::Struct proto_config_;
  ConfigPerRouteInterfaceSharedPtr config_;
  mutable std::mutex mutex_;
};
} // namespace SrhinoPluginFramework