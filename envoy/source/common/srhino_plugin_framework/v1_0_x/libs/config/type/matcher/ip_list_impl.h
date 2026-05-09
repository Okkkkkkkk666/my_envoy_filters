#pragma once

#include <memory>
#include "source/common/network/cidr_range.h"
#include "source/common/network/utility.h"
#include "envoy/srhino_plugin_framework/v1_0_x/libs/config/type/matcher/ip_list.h"
#include "source/common/network/address_impl.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
class IpRangeImpl : public IpRange {
public:
  IpRangeImpl(
      const srhino_plugin_framework::v1_0_x::proto::config::type::matcher::IpRange& ip_range);

public:
  bool isValid() const;
  bool isInRange(uint32_t address) const override;

private:
  const Envoy::Network::Address::InstanceConstSharedPtr start_ip_;
  const Envoy::Network::Address::InstanceConstSharedPtr end_ip_;
};

using IpRangePtr = std::unique_ptr<const IpRangeImpl>;

class IpSetImpl : public IpSet {
public:
  IpSetImpl(const srhino_plugin_framework::v1_0_x::proto::config::type::matcher::IpSet& ip_set);

public:
  bool isInRange(uint32_t address) const override;
  uint64_t hash() const { return hash_code_; }

private:
  const std::vector<Envoy::Network::Address::CidrRange> cidr_ranges_;
  uint64_t hash_code_;
};
using IpSetPtr = std::unique_ptr<const IpSetImpl>;
} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework