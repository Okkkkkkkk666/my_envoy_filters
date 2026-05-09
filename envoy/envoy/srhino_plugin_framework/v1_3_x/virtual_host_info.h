#pragma once

#include "route_info.h"
#include "../config.h"
#include "utility/proto_tools.hpp"

namespace SrhinoPluginFramework {
namespace v1_3_x {
static std::mutex VirutalHostInfoMtx;
class VirtualHostInfo {
public:
  virtual ~VirtualHostInfo() = default;

public:
  virtual const std::string& name() const = 0;

public:
  virtual void* config() const = 0;
};
} // namespace v1_3_x
} // namespace SrhinoPluginFramework