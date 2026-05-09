#pragma once

#include "envoy/srhino_plugin_framework/v1_4_x/libs/config/core/cidr.h"
#include "source/common/network/cidr_range.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Config {
namespace Core {
class CidrImpl : public Cidr {
public:
  CidrImpl(const srhino_plugin_framework::v1_4_x::proto::config::core::CidrRange& config);

private:
  Envoy::Network::Address::CidrRange cidr_range_;
};
} // namespace Core
} // namespace Config
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework