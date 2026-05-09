#pragma once

#include <string>
#include <set>

#include "envoy/network/address.h"
#include "envoy/http/header_map.h"

#include "filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3/strong_global_ratelimit.pb.h"

#include "filters/source/extensions/filters/http/common/strong_matcher/matcher.h"

#include "action.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongGlobalRatelimit {
namespace Impl {

class Rule {
public:
  Rule(const v3::Rule& rule);
  Rule(const Rule& rule) = delete;

public:
  /**
   * 判断本规则是否匹配
   * @param headers HTTP头
   * @param address 下游地址
   * @param user_name 用户名
   * @return true
   * @return false
   */
  bool match(const Http::RequestHeaderMap& headers,
             const Network::Address::InstanceConstSharedPtr& address,
             const std::string& user_name) const;

  /**
   * 获取本规则hash值
   * @return uint64_t
   */
  uint64_t hash() const { return hash_code_; }

  // PB 属性
public:
  const Action& action() const { return action_; }
  const std::string& ruleName() { return rule_name_; }
  bool hasUserNameMatch() const;

private:
  const std::vector<std::unique_ptr<Filters::Common::StrongMatcher::Matcher>> matchers_;
  const std::string rule_name_;
  const Action action_;
  const uint64_t hash_code_;

  // 计算hash
private:
  uint64_t calcHash() const;
  inline void updateHashWithMatchers(uint64_t& hash_code) const;
  inline void updateHashWithAction(uint64_t& hash_code) const;
};

using RulePtr = std::unique_ptr<Rule>;

} // namespace Impl
} // namespace StrongGlobalRatelimit
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
