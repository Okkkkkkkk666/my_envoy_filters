#include "source/common/srhino_plugin_framework/v1_3_x/libs/config/type/matcher/rule_impl.h"
#include "source/common/srhino_plugin_framework/v1_3_x/header_map_impl.h"
#include "envoy/srhino_plugin_framework/v1_3_x/utility/proto_tools.hpp"
#include "ip_list_impl.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
IpRangeImpl::IpRangeImpl(
    const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::IpRange& ip_range)
    : start_ip_(Envoy::Network::Utility::parseInternetAddress(ip_range.start_ip())),
      end_ip_(Envoy::Network::Utility::parseInternetAddress([&ip_range]() {
        // 支持解析如1.1.1.1-100的ip范围
        if (ip_range.end_ip().find(".") == std::string::npos) {
          size_t last_dot = ip_range.start_ip().rfind(".");
          absl::string_view start_ip_prefix_view(ip_range.start_ip().substr(0, last_dot));
          return std::string(start_ip_prefix_view) + "." + ip_range.end_ip();
        }
        return ip_range.end_ip();
      }())) {}

bool IpRangeImpl::isValid() const {
  if (start_ip_->ip()->version() != end_ip_->ip()->version()) {
    return false;
  }
  switch (start_ip_->ip()->version()) {
  case Envoy::Network::Address::IpVersion::v4: {
    uint32_t start_ip_int = ntohl(start_ip_->ip()->ipv4()->address());
    uint32_t end_ip_int = ntohl(end_ip_->ip()->ipv4()->address());
    // 确保起始IP小于等于结束IP
    return start_ip_int <= end_ip_int; 
  }
  case Envoy::Network::Address::IpVersion::v6: {
    absl::uint128 start_ip_int =
        Envoy::Network::Utility::Ip6ntohl(start_ip_->ip()->ipv6()->address());
    absl::uint128 end_ip_int = Envoy::Network::Utility::Ip6ntohl(end_ip_->ip()->ipv6()->address());
    return start_ip_int <= end_ip_int;
  }
  default:
    return false;
  }
}

static Envoy::Network::Address::InstanceConstSharedPtr convertProtoConfig(uint32_t address) {
  sockaddr_in sock_address = {};
  sock_address.sin_addr.s_addr = address;

  return std::make_shared<Envoy::Network::Address::Ipv4Instance>(&sock_address);
}

static envoy::config::core::v3::CidrRange
convertProtoConfig(const srhino_plugin_framework::v1_3_x::proto::config::core::CidrRange& config) {
  envoy::config::core::v3::CidrRange v3_config;
  Utility::ProtoTools::convertMessage(config, v3_config);
  return v3_config;
}

bool IpRangeImpl::isInRange(uint32_t address) const {

  auto addr = convertProtoConfig(address);
  if (!isValid() || addr->type() != Envoy::Network::Address::Type::Ip ||
      start_ip_->ip()->version() != addr->ip()->version() ||
      end_ip_->ip()->version() != addr->ip()->version()) {
    return false;
  }

  switch (addr->ip()->version()) {
  case Envoy::Network::Address::IpVersion::v4: {
    uint32_t addr_int = ntohl(addr->ip()->ipv4()->address());
    uint32_t start_ip_int = ntohl(start_ip_->ip()->ipv4()->address());
    uint32_t end_ip_int = ntohl(end_ip_->ip()->ipv4()->address());
    return addr_int >= start_ip_int && addr_int <= end_ip_int;
  }
  case Envoy::Network::Address::IpVersion::v6: {
    absl::uint128 addr_int = Envoy::Network::Utility::Ip6ntohl(addr->ip()->ipv6()->address());
    absl::uint128 start_ip_int =
        Envoy::Network::Utility::Ip6ntohl(start_ip_->ip()->ipv6()->address());
    absl::uint128 end_ip_int = Envoy::Network::Utility::Ip6ntohl(end_ip_->ip()->ipv6()->address());
    return addr_int >= start_ip_int && addr_int <= end_ip_int;
  }
  default:
    return false;
  }
}

IpSetImpl::IpSetImpl(
    const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::IpSet& ip_set)
    : cidr_ranges_([&ip_set, this]() {
        std::vector<Envoy::Network::Address::CidrRange> cidr_ranges;
        for (auto& cidr : ip_set.list()) {
          cidr_ranges.emplace_back(
              Envoy::Network::Address::CidrRange::create(convertProtoConfig(cidr)));
        }
        return cidr_ranges;
      }()) {}

bool IpSetImpl::isInRange(uint32_t address) const {
  auto addr = convertProtoConfig(address);
  for (const auto& cidr : cidr_ranges_) {
    if (cidr.isInRange(*addr)) {
      return true;
    }
  }
  return false;
}

IPGroupsImpl::IPGroupsImpl(
    const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::IPGroups& ip_groups)
    : ip_list_(buildIpSet(ip_groups)), ip_range_(buildIpRangeVector(ip_groups)),
      ip_invert_(ip_groups.invert()) {}

bool SrhinoPluginFramework::v1_3_x::Libs::Config::Type::Matcher::IPGroupsImpl::isInRange(
    uint32_t address) const {
  bool is_match = false;
  if (ip_range_.empty() && ip_list_ == nullptr) {
    return true;
  }
  // IpRange匹配
  if (!ip_range_.empty()) {
    for (const auto& ip_range : ip_range_) {
      if (ip_range->isInRange(address)) {
        is_match = true;
        break;
      }
    }
  }
  // ipset匹配
  if (!is_match) {
    if (ip_list_ != nullptr) {
      if (ip_list_->isInRange(address)) {
        is_match = true;
      }
    }
  }

  return is_match;
}

std::vector<IpRangePtr> IPGroupsImpl::buildIpRangeVector(
    const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::IPGroups& ip_groups)
    const {
  std::vector<IpRangePtr> matchers;
  for (const auto& ip_range : ip_groups.ip_range()) {
    matchers.emplace_back(std::make_unique<IpRangeImpl>(ip_range));
  }
  return matchers;
}

IpSetPtr IPGroupsImpl::buildIpSet(
    const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::IPGroups& ip_groups)
    const {
  if (ip_groups.has_ip_set()) {
    return std::make_unique<IpSetImpl>(ip_groups.ip_set());
  }
  return nullptr;
}

} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework