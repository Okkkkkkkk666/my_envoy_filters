#pragma once

#include "source/common/network/cidr_range.h"
#include "source/common/network/utility.h"

#include "filters/api/envoy/extensions/filters/http/common/strong_matcher/v3/matcher.pb.h"
#include "filters/api/envoy/extensions/filters/http/common/ip_whitelist/v3/ip_whitelist.pb.h"

#include "filters/source/extensions/filters/http/common/strong_matcher/matcher.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace IpWhitelist {

namespace v3 = envoy::extensions::filters::http::common::ip_whitelist::v3;
namespace matcher_v3 = envoy::extensions::filters::http::common::strong_matcher::v3;

class IpRange {
public:
  IpRange(const v3::IpRange& ip_range);
  bool isValid() const;
  bool isInRange(const Network::Address::Instance& address) const;

private:
  const Network::Address::InstanceConstSharedPtr start_ip_;
  const Network::Address::InstanceConstSharedPtr end_ip_;
};
using IpRangePtr = std::unique_ptr<IpRange>;

class IpWhitelist {
public:
  IpWhitelist(const v3::IpWhitelist& ip_whitelist);
  IpWhitelist(const v3::IpRange& ip_set, bool enable);
  bool match(const Network::Address::InstanceConstSharedPtr& address) const;
  const std::unique_ptr<StrongMatcher::IpSet>& srcIp() const { return ip_list_; }
  const std::vector<IpRangePtr>& ipRange() const { return ip_range_list_; }
  bool enable() const { return enable_; }

private:
  const std::unique_ptr<StrongMatcher::IpSet> ip_list_;
  const std::vector<IpRangePtr> ip_range_list_;
  const bool enable_;
  const std::string name_;
};

using IpWhitelistPtr = std::unique_ptr<IpWhitelist>;

} // namespace IpWhitelist
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy