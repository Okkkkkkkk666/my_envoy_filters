#pragma once

#include "../config.h"
#include "utility/proto_tools.hpp"

namespace SrhinoPluginFramework {
namespace v1_4_x {
static std::mutex RouteInfoMtx;
class RouteInfo {
public:
  virtual ~RouteInfo() = default;

public:
  virtual const std::string& name() const = 0;
  virtual const std::string& filterName() const = 0;

public:
  virtual void* config() const = 0;
};
} // namespace v1_4_x
} // namespace SrhinoPluginFramework