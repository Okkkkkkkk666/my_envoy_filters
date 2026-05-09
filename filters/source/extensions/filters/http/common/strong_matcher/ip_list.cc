#include "ip_list.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace StrongMatcher {

IpRange::IpRange(const v3::IpRange& ip_range)
    : start_ip_(Network::Utility::parseInternetAddress(ip_range.start_ip())),
      end_ip_(Network::Utility::parseInternetAddress([&ip_range]() {
        // 支持解析如1.1.1.1-100的ip范围
        if (ip_range.end_ip().find(".") == std::string::npos) {
          size_t last_dot = ip_range.start_ip().rfind(".");
          absl::string_view start_ip_prefix_view(ip_range.start_ip().substr(0, last_dot));
          return std::string(start_ip_prefix_view) + "." + ip_range.end_ip();
        }
        return ip_range.end_ip();
      }())) {}

/**
 * 有效ip范围检测
 */
bool IpRange::isValid() const {
  if (start_ip_->ip()->version() != end_ip_->ip()->version()) {
    return false;
  }
  switch (start_ip_->ip()->version()) {
  case Network::Address::IpVersion::v4:
    if (ntohl(start_ip_->ip()->ipv4()->address()) >> 8 ==
            ntohl(end_ip_->ip()->ipv4()->address()) >> 8 &&
        ntohl(start_ip_->ip()->ipv4()->address()) <= ntohl(end_ip_->ip()->ipv4()->address())) {
      return true;
    }
    break;

  case Network::Address::IpVersion::v6:
    if (Network::Utility::Ip6ntohl(start_ip_->ip()->ipv6()->address()) >> 16 ==
            Network::Utility::Ip6ntohl(end_ip_->ip()->ipv6()->address()) >> 16 &&
        Network::Utility::Ip6ntohl(start_ip_->ip()->ipv6()->address()) <=
            Network::Utility::Ip6ntohl(end_ip_->ip()->ipv6()->address())) {
      return true;
    }
    break;
  }
  return false;
}

bool IpRange::isInRange(const Network::Address::Instance& address) const {

  if (!isValid() || address.type() != Network::Address::Type::Ip ||
      start_ip_->ip()->version() != address.ip()->version() ||
      end_ip_->ip()->version() != address.ip()->version()) {
    return false;
  }

  switch (address.ip()->version()) {
  case Network::Address::IpVersion::v4:
    if (ntohl(address.ip()->ipv4()->address()) >= ntohl(start_ip_->ip()->ipv4()->address()) &&
        ntohl(address.ip()->ipv4()->address()) <= ntohl(end_ip_->ip()->ipv4()->address())) {
      return true;
    }
    break;

  case Network::Address::IpVersion::v6:
    if (Network::Utility::Ip6ntohl(address.ip()->ipv6()->address()) >=
            Network::Utility::Ip6ntohl(start_ip_->ip()->ipv6()->address()) &&
        Network::Utility::Ip6ntohl(address.ip()->ipv6()->address()) <=
            Network::Utility::Ip6ntohl(end_ip_->ip()->ipv6()->address())) {
      return true;
    }
    break;
  }
  return false;
}

IpSet::IpSet(const v3::IpSet& ip_set)
    : name_(ip_set.name()), desc_(ip_set.desc()), cidr_ranges_([&ip_set]() {
        std::vector<Network::Address::CidrRange> cidr_ranges;
        for (auto& cidr : ip_set.list()) {
          cidr_ranges.emplace_back(Network::Address::CidrRange::create(cidr));
        }
        return cidr_ranges;
      }()) {}

bool IpSet::isInRange(const Network::Address::Instance& address) const {
  for (const auto& cidr : cidr_ranges_) {
    if (cidr.isInRange(address)) {
      return true;
    }
  }
  return false;
}

} // namespace StrongMatcher
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy