#pragma once
#include "matcher.h"
#include "ip_list.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace StrongMatcher {

class Rule {
public:
  Rule(const std::string& upstream, const envoy::config::core::v3::CidrRange& cidr, bool ip_invert,
       const google::protobuf::RepeatedPtrField<v3::Matcher>& matchers);
  Rule(const std::string& upstream, const v3::IpSet& ip_set, bool ip_invert,
       const google::protobuf::RepeatedPtrField<v3::Matcher>& matchers);
  Rule(const std::string& upstream,
       const google::protobuf::RepeatedPtrField<v3::Matcher>& matchers);
  Rule(const Rule&) = delete;

public:
  /**
   * 判断本规则是否匹配
   * @param upstream 上游集群名
   * @param address 下游地址
   * @param headers HTTP头
   * @param pass_empty 所有匹配条件为空时，视为匹配
   * @return true
   * @return false
   */
  bool match(const std::string& upstream, const Network::Address::InstanceConstSharedPtr& address,
             const Http::RequestHeaderMap& headers, bool pass_empty) const;

  /**
   * 获取本规则hash值
   * @return uint64_t
   */
  u_int64_t hash() const { return hash_code_; }

  // 属性
public:
  const std::string& upstreamName() const { return upstream_name_; }
  const absl::optional<Network::Address::CidrRange>& srcIpRange() const { return src_ip_range_; }
  const absl::optional<IpSet>& ipSet() const { return ip_set_; }
  bool ipInvert() const { return ip_invert_; }
  const std::vector<Matcher>& matchers() { return matchers_; }

private:
  const std::string upstream_name_;
  const absl::optional<Network::Address::CidrRange> src_ip_range_;
  const absl::optional<IpSet> ip_set_;
  const bool ip_invert_;
  const std::vector<Matcher> matchers_;
  const uint64_t hash_code_;

  // 计算hash
private:
  uint64_t calcHash() const;
  inline void updateHashWithUpstreamName(uint64_t& hash_code) const;
  inline void updateHashWithSrcIp(uint64_t& hash_code) const;
  inline void updateHashWithIpSet(uint64_t& hash_code) const;
  inline void updateHashWithMatchers(uint64_t& hash_code) const;
};
} // namespace StrongMatcher
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy