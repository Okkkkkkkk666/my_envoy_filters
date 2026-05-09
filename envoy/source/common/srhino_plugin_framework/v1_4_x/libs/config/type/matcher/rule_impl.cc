#include "source/common/srhino_plugin_framework/v1_4_x/libs/config/type/matcher/rule_impl.h"
#include "source/common/srhino_plugin_framework/v1_4_x/header_map_impl.h"
#include "envoy/srhino_plugin_framework/v1_4_x/utility/proto_tools.hpp"
#include "rule_impl.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
RuleImpl::RuleImpl(
    const std::string& upstream_name,
    const srhino_plugin_framework::v1_4_x::proto::config::core::CidrRange& cidr, bool ip_invert,
    const google::protobuf::RepeatedPtrField<
        srhino_plugin_framework::v1_4_x::proto::config::type::matcher::Matcher>& matchers)
    : Rule(upstream_name, cidr, ip_invert, matchers),upstream_name_(upstream_name),
      src_ip_range_(absl::optional<Envoy::Network::Address::CidrRange>(
          Envoy::Network::Address::CidrRange::create(convertProtoConfig(cidr)))),
      ip_set_(absl::nullopt), ip_invert_(ip_invert), matchers_([&matchers]() {
        std::vector<MatcherImpl> result;
        for (const srhino_plugin_framework::v1_4_x::proto::config::type::matcher::Matcher& matcher :
             matchers) {
          result.emplace_back(matcher);
        }
        return result;
      }()),
      hash_code_(calcHash()) {}

RuleImpl::RuleImpl(
    const std::string& upstream_name,
    const srhino_plugin_framework::v1_4_x::proto::config::type::matcher::IpSet& ip_set,
    bool ip_invert,
    const google::protobuf::RepeatedPtrField<
        srhino_plugin_framework::v1_4_x::proto::config::type::matcher::Matcher>& matchers)
    : Rule(upstream_name, ip_set, ip_invert, matchers),upstream_name_(upstream_name), src_ip_range_(absl::nullopt),
      ip_set_(absl::optional<IpSetImpl>(IpSetImpl(ip_set))), ip_invert_(ip_invert),
      matchers_([&matchers]() {
        std::vector<MatcherImpl> result;
        for (const srhino_plugin_framework::v1_4_x::proto::config::type::matcher::Matcher& matcher :
             matchers) {
          result.emplace_back(matcher);
        }
        return result;
      }()),
      hash_code_(calcHash()) {}

RuleImpl::RuleImpl(
    const std::string& upstream_name,
    const google::protobuf::RepeatedPtrField<
        srhino_plugin_framework::v1_4_x::proto::config::type::matcher::Matcher>& matchers)
    : Rule(upstream_name, matchers), upstream_name_(upstream_name), src_ip_range_(absl::nullopt), ip_set_(absl::nullopt),
      ip_invert_(false), matchers_([&matchers]() {
        std::vector<MatcherImpl> result;
        for (const srhino_plugin_framework::v1_4_x::proto::config::type::matcher::Matcher& matcher :
             matchers) {
          result.emplace_back(matcher);
        }
        return result;
      }()),
      hash_code_(calcHash()) {}

bool RuleImpl::match(const std::string& upstream, uint32_t address,
                     const HeaderContext& context, bool pass_empty) const {
  bool has_config = false;

  // upstream_name_
  if (!upstream_name_.empty()) {
    has_config = true;
    if (upstream != upstream_name_) {
      return false;
    }
  }

  // src_ip_range_
  if (src_ip_range_.has_value()) {
    has_config = true;
    bool is_match = src_ip_range_->isInRange(*convertProtoConfig(address).get());
    if (ip_invert_) {
      is_match = !is_match;
    }

    if (!is_match) {
      return false;
    }
  }

  // ip_set_
  if (ip_set_.has_value()) {
    has_config = true;
    bool is_match = ip_set_->isInRange(address);
    if (ip_invert_) {
      is_match = !is_match;
    }

    if (!is_match) {
      return false;
    }
  }

  // matchers_
  if (!matchers_.empty()) {
    has_config = true;
    bool is_match = false;
    for (auto& matcher : matchers_) {
      is_match = matcher.matchAll(context);
      if (is_match) {
        break;
      }
    }

    if (!is_match) {
      return false;
    }
  }

  return pass_empty ? true : has_config;
}


uint64_t RuleImpl::calcHash() const {
  uint64_t hash_code = 0;

  updateHashWithUpstreamName(hash_code);
  updateHashWithSrcIp(hash_code);
  updateHashWithIpSet(hash_code);
  updateHashWithMatchers(hash_code);

  return hash_code;
}

inline void RuleImpl::updateHashWithUpstreamName(uint64_t& hash_code) const {
  MatcherImpl::hashUpdate(upstream_name_.c_str(), upstream_name_.size(), hash_code);
}

inline void RuleImpl::updateHashWithSrcIp(uint64_t& hash_code) const {
  if (src_ip_range_.has_value()) {
    if (src_ip_range_->ip()->version() == Envoy::Network::Address::IpVersion::v4) {
      uint32_t ip = src_ip_range_->ip()->ipv4()->address();
      MatcherImpl::hashUpdate(&ip, sizeof(ip), hash_code);
    } else {
      absl::uint128 ip = src_ip_range_->ip()->ipv6()->address();
      MatcherImpl::hashUpdate(&ip, sizeof(ip), hash_code);
    }

    uint32_t len = src_ip_range_->length();
    MatcherImpl::hashUpdate(&len, sizeof(len), hash_code);
    MatcherImpl::hashUpdate(ip_invert_, hash_code);
  }
}

inline void RuleImpl::updateHashWithIpSet(uint64_t& hash_code) const {
  if (ip_set_.has_value()) {
    MatcherImpl::hashUpdate(ip_set_.value().hash(), hash_code);
    MatcherImpl::hashUpdate(ip_invert_, hash_code);
  }
}

inline void RuleImpl::updateHashWithMatchers(uint64_t& hash_code) const {
  for (const auto& matcher : matchers_) {
    uint64_t tmp = matcher.hash();
    MatcherImpl::hashUpdate(&tmp, sizeof(tmp), hash_code);
  }
}

envoy::config::core::v3::CidrRange RuleImpl::convertProtoConfig(
    const srhino_plugin_framework::v1_4_x::proto::config::core::CidrRange& config) const {
  envoy::config::core::v3::CidrRange v3_config;
  Utility::ProtoTools::convertMessage(config, v3_config);
  return v3_config;
}

Envoy::Network::Address::InstanceConstSharedPtr
RuleImpl::convertProtoConfig(uint32_t address) const {
  sockaddr_in sock_address = {};
  sock_address.sin_addr.s_addr = address;
  return std::make_shared<Envoy::Network::Address::Ipv4Instance>(&sock_address);
}

} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework