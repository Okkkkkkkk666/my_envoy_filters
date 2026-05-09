#include "rule.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace StrongMatcher {

Rule::Rule(const std::string& upstream, const envoy::config::core::v3::CidrRange& cidr,
           bool ip_invert, const google::protobuf::RepeatedPtrField<v3::Matcher>& matchers)
    : upstream_name_(upstream), src_ip_range_(absl::optional<Network::Address::CidrRange>(
                                    Network::Address::CidrRange::create(cidr))),
      ip_set_(absl::nullopt), ip_invert_(ip_invert), matchers_([&matchers]() {
        std::vector<Filters::Common::StrongMatcher::Matcher> result;
        for (const envoy::extensions::filters::http::common::strong_matcher::v3::Matcher& matcher :
             matchers) {
          result.emplace_back(matcher);
        }
        return result;
      }()),
      hash_code_(calcHash()) {}

Rule::Rule(const std::string& upstream, const v3::IpSet& ip_set, bool ip_invert,
           const google::protobuf::RepeatedPtrField<v3::Matcher>& matchers)
    : upstream_name_(upstream), src_ip_range_(absl::nullopt),
      ip_set_(absl::optional<IpSet>(IpSet(ip_set))), ip_invert_(ip_invert),
      matchers_([&matchers]() {
        std::vector<Filters::Common::StrongMatcher::Matcher> result;
        for (const envoy::extensions::filters::http::common::strong_matcher::v3::Matcher& matcher :
             matchers) {
          result.emplace_back(matcher);
        }
        return result;
      }()),
      hash_code_(calcHash()) {}

Rule::Rule(const std::string& upstream, const google::protobuf::RepeatedPtrField<v3::Matcher>& matchers)
    : upstream_name_(upstream), src_ip_range_(absl::nullopt),
      ip_set_(absl::nullopt), ip_invert_(false),
      matchers_([&matchers]() {
        std::vector<Filters::Common::StrongMatcher::Matcher> result;
        for (const envoy::extensions::filters::http::common::strong_matcher::v3::Matcher& matcher :
             matchers) {
          result.emplace_back(matcher);
        }
        return result;
      }()),
      hash_code_(calcHash()) {}

bool Rule::match(const std::string& upstream,
                 const Network::Address::InstanceConstSharedPtr& address,
                 const Http::RequestHeaderMap& headers, bool pass_empty) const {
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
    bool is_match = src_ip_range_->isInRange(*address.get());
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
    bool is_match = ip_set_->isInRange(*address.get());
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
      is_match = matcher.matchAll(headers);
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

uint64_t Rule::calcHash() const {
  uint64_t hash_code = 0;

  updateHashWithUpstreamName(hash_code);
  updateHashWithSrcIp(hash_code);
  updateHashWithIpSet(hash_code);
  updateHashWithMatchers(hash_code);

  return hash_code;
}

inline void Rule::updateHashWithUpstreamName(uint64_t& hash_code) const {
  Matcher::hashUpdate(upstream_name_.c_str(), upstream_name_.size(), hash_code);
}

inline void Rule::updateHashWithSrcIp(uint64_t& hash_code) const {
  if (src_ip_range_.has_value()) {
    if (src_ip_range_->ip()->version() == Network::Address::IpVersion::v4) {
      uint32_t ip = src_ip_range_->ip()->ipv4()->address();
      Matcher::hashUpdate(&ip, sizeof(ip), hash_code);
    } else {
      absl::uint128 ip = src_ip_range_->ip()->ipv6()->address();
      Matcher::hashUpdate(&ip, sizeof(ip), hash_code);
    }

    uint32_t len = src_ip_range_->length();
    Matcher::hashUpdate(&len, sizeof(len), hash_code);
    Matcher::hashUpdate(ip_invert_, hash_code);
  }
}

inline void Rule::updateHashWithIpSet(uint64_t& hash_code) const {
  if (ip_set_.has_value()) {
    Matcher::hashUpdate(ip_set_.value().hash(), hash_code);
    Matcher::hashUpdate(ip_invert_, hash_code);
  }
}

inline void Rule::updateHashWithMatchers(uint64_t& hash_code) const {
  for (const auto& matcher : matchers_) {
    uint64_t tmp = matcher.hash();
    Matcher::hashUpdate(&tmp, sizeof(tmp), hash_code);
  }
}

} // namespace StrongMatcher
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
