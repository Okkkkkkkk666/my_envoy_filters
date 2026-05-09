#pragma once

#include "source/common/network/cidr_range.h"
#include "source/common/router/config_utility.h"
#include "source/common/network/utility.h"
#include "filters/api/envoy/extensions/filters/http/common/strong_matcher/v3/matcher.pb.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace StrongMatcher {

namespace v3 = envoy::extensions::filters::http::common::strong_matcher::v3;
class IpRange {
public:
  IpRange(const v3::IpRange& ip_range);
  bool isValid() const;
  bool isInRange(const Network::Address::Instance& address) const;

private:
  const Network::Address::InstanceConstSharedPtr start_ip_;
  const Network::Address::InstanceConstSharedPtr end_ip_;
};

using IpRangePtr = std::unique_ptr<const IpRange>;

class IpSet {
public:
  IpSet(const v3::IpSet& ip_set);

public:
  bool isInRange(const Network::Address::Instance& address) const;
  uint64_t hash() const { return hash_code_; }

private:
  const std::string name_;
  const std::string desc_;
  const std::vector<Network::Address::CidrRange> cidr_ranges_;
  uint64_t hash_code_;
};

using IpSetPtr = std::unique_ptr<const IpSet>;

} // namespace StrongMatcher
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy