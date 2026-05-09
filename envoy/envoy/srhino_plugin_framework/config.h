#pragma once

#include <memory>

namespace SrhinoPluginFramework {
class ConfigPerRouteInterface {
public:
  virtual ~ConfigPerRouteInterface() = default;
};

using ConfigPerRouteInterfaceSharedPtr = std::shared_ptr<ConfigPerRouteInterface>;
} // namespace SrhinoPluginFramework