#include "source/common/srhino_plugin_framework/v1_0_x/libs/config/type/matcher/rule_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/header_map_impl.h"
#include "envoy/srhino_plugin_framework/v1_0_x/utility/proto_tools.hpp"
#include "ip_list_impl.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
IpRangeImpl::IpRangeImpl(
    const srhino_plugin_framework::v1_0_x::proto::config::type::matcher::IpRange& ip_range)
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
  case Envoy::Network::Address::IpVersion::v4:
    if (ntohl(start_ip_->ip()->ipv4()->address()) >> 8 ==
            ntohl(end_ip_->ip()->ipv4()->address()) >> 8 &&
        ntohl(start_ip_->ip()->ipv4()->address()) <= ntohl(end_ip_->ip()->ipv4()->address())) {
      return true;
    }
    break;

  case Envoy::Network::Address::IpVersion::v6:
    if (Envoy::Network::Utility::Ip6ntohl(start_ip_->ip()->ipv6()->address()) >> 16 ==
            Envoy::Network::Utility::Ip6ntohl(end_ip_->ip()->ipv6()->address()) >> 16 &&
        Envoy::Network::Utility::Ip6ntohl(start_ip_->ip()->ipv6()->address()) <=
            Envoy::Network::Utility::Ip6ntohl(end_ip_->ip()->ipv6()->address())) {
      return true;
    }
    break;
  }
  return false;
}


static Envoy::Network::Address::InstanceConstSharedPtr convertProtoConfig(uint32_t address) {
  sockaddr_in sock_address = {};
  sock_address.sin_addr.s_addr = address;

  return std::make_shared<Envoy::Network::Address::Ipv4Instance>(&sock_address);
}

static envoy::config::core::v3::CidrRange convertProtoConfig(
    const srhino_plugin_framework::v1_0_x::proto::config::core::CidrRange& config)  {
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
  case Envoy::Network::Address::IpVersion::v4:
    if (ntohl(addr->ip()->ipv4()->address()) >= ntohl(start_ip_->ip()->ipv4()->address()) &&
        ntohl(addr->ip()->ipv4()->address()) <= ntohl(end_ip_->ip()->ipv4()->address())) {
      return true;
    }
    break;

  case Envoy::Network::Address::IpVersion::v6:
    if (Envoy::Network::Utility::Ip6ntohl(addr->ip()->ipv6()->address()) >=
            Envoy::Network::Utility::Ip6ntohl(start_ip_->ip()->ipv6()->address()) &&
        Envoy::Network::Utility::Ip6ntohl(addr->ip()->ipv6()->address()) <=
            Envoy::Network::Utility::Ip6ntohl(end_ip_->ip()->ipv6()->address())) {
      return true;
    }
    break;
  }
  return false;
}

IpSetImpl::IpSetImpl(
    const srhino_plugin_framework::v1_0_x::proto::config::type::matcher::IpSet& ip_set)
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

} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework