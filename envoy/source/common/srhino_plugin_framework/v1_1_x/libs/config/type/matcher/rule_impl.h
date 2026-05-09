#pragma once

#include "envoy/srhino_plugin_framework/v1_1_x/libs/config/type/matcher/rule.h"
#include "source/common/network/address_impl.h"
#include "matcher_impl.h"
namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
class RuleImpl : public Rule {
public:
  RuleImpl(const std::string& upstream_name,
           const srhino_plugin_framework::v1_1_x::proto::config::core::CidrRange& cidr,
           bool ip_invert,
           const google::protobuf::RepeatedPtrField<
               srhino_plugin_framework::v1_1_x::proto::config::type::matcher::Matcher>& matchers);
  RuleImpl(const std::string& upstream_name,
           const srhino_plugin_framework::v1_1_x::proto::config::type::matcher::IpSet& ip_set,
           bool ip_invert,
           const google::protobuf::RepeatedPtrField<
               srhino_plugin_framework::v1_1_x::proto::config::type::matcher::Matcher>& matchers);
  RuleImpl(const std::string& upstream_name,
           const google::protobuf::RepeatedPtrField<
               srhino_plugin_framework::v1_1_x::proto::config::type::matcher::Matcher>& matchers);

public:
  bool match(const std::string& upstream_name, uint32_t address, const HeaderContext& context,
             bool pass_empty) const override;

// 属性
public:
  const std::string& upstreamName() const { return upstream_name_; }
  const absl::optional<Envoy::Network::Address::CidrRange>& srcIpRange() const { return src_ip_range_; }
  const absl::optional<IpSetImpl>& ipSet() const { return ip_set_; }
  bool ipInvert() const { return ip_invert_; }
  const std::vector<MatcherImpl>& matchers() { return matchers_; }

private:
  const std::string upstream_name_;
  const absl::optional<Envoy::Network::Address::CidrRange> src_ip_range_;
  const absl::optional<IpSetImpl> ip_set_;
  const bool ip_invert_;
  const std::vector<MatcherImpl> matchers_;
  const uint64_t hash_code_;
  // 计算hash
private:
  uint64_t calcHash() const;
  inline void updateHashWithUpstreamName(uint64_t& hash_code) const;
  inline void updateHashWithSrcIp(uint64_t& hash_code) const;
  inline void updateHashWithIpSet(uint64_t& hash_code) const;
  inline void updateHashWithMatchers(uint64_t& hash_code) const;

private:
  envoy::config::core::v3::CidrRange convertProtoConfig(
      const srhino_plugin_framework::v1_1_x::proto::config::core::CidrRange& config) const;

  Envoy::Network::Address::InstanceConstSharedPtr convertProtoConfig(uint32_t address) const;
};
} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework