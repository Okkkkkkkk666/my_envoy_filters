#pragma once

#include "../config.h"
#include "utility/proto_tools.hpp"

namespace SrhinoPluginFramework {
namespace v1_2_x {
static std::mutex RouteInfoMtx;
class RouteInfo {
public:
  virtual ~RouteInfo() = default;

public:
  template <class ProtoConfigT, class RuntimeConfigT> RuntimeConfigT* config() const {
    ConfigPerRouteInterface* config = configInternal();
    if (!config) {
      std::unique_lock<std::mutex> lock(RouteInfoMtx);
      config = configInternal();
      if (config) {
        return dynamic_cast<RuntimeConfigT*>(config);
      }
      std::string json = configJsonString();
      ProtoConfigT proto_config;
      std::string error = Utility::ProtoTools::jsonToMessage(json, proto_config);
      if (error.empty()) {
        std::shared_ptr<RuntimeConfigT> runtime_config =
            std::make_shared<RuntimeConfigT>(std::move(proto_config));
        setConfig(std::dynamic_pointer_cast<ConfigPerRouteInterface>(runtime_config));
        return runtime_config.get();
      }
    } else {
      return dynamic_cast<RuntimeConfigT*>(config);
    }

    return nullptr;
  }

public:
  virtual const std::string& name() const = 0;

protected:
  virtual ConfigPerRouteInterface* configInternal() const = 0;
  virtual std::string configJsonString() const = 0;
  virtual void setConfig(ConfigPerRouteInterfaceSharedPtr config) const = 0;
};
} // namespace v1_2_x
} // namespace SrhinoPluginFramework