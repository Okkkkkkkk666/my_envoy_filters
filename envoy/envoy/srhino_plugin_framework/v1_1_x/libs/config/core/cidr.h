#pragma once

#include "envoy/srhino_plugin_framework/v1_1_x/proto/config/core/cidr.pb.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Config {
namespace Core {
class Cidr {
public:
  virtual ~Cidr() = default;

public:
  virtual const srhino_plugin_framework::v1_1_x::proto::config::core::CidrRange& config() const = 0;

public:
  virtual bool isInRange(std::uint32_t address) = 0;
};
} // namespace Core
} // namespace Config
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework