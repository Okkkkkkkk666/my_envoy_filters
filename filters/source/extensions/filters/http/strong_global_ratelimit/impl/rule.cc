#include "source/common/http/path_utility.h"

#include "rule.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongGlobalRatelimit {
namespace Impl {
Rule::Rule(const v3::Rule& rule)
    : matchers_([&rule]() {
        std::vector<std::unique_ptr<Filters::Common::StrongMatcher::Matcher>> result;
        for (const auto& matcher : rule.condition()) {
          result.emplace_back(std::make_unique<Filters::Common::StrongMatcher::Matcher>(matcher));
        }
        return result;
      }()),
      rule_name_(rule.rule_name()), action_(rule.action()),
      hash_code_(calcHash()) {}

bool Rule::match(const Http::RequestHeaderMap& headers,
                 const Network::Address::InstanceConstSharedPtr& address,
                 const std::string& user_name) const {

  for (auto& matcher : matchers_) {
    if (!matcher->matchAll(headers, address, user_name)) {
      return false;
    }
  }
  return true;
}

uint64_t Rule::calcHash() const {
  uint64_t hash_code = 0;

  updateHashWithMatchers(hash_code);
  updateHashWithAction(hash_code);

  return hash_code;
}

inline void Rule::updateHashWithMatchers(uint64_t& hash_code) const {
  using Filters::Common::StrongMatcher::Matcher;

  for (const auto& matcher : matchers_) {
    uint64_t tmp = matcher->hash();
    Matcher::hashUpdate(&tmp, sizeof(tmp), hash_code);
  }
}

inline void Rule::updateHashWithAction(uint64_t& hash_code) const {
  using Filters::Common::StrongMatcher::Matcher;

  auto dryrun = action_.dryrun();
  auto proc_next_rule = action_.procNextRule();
  Matcher::hashUpdate(&dryrun, sizeof(dryrun), hash_code);
  Matcher::hashUpdate(&proc_next_rule, sizeof(proc_next_rule), hash_code);
  for (const Filters::Common::RatelimitClient::Impl::Quota& quota : action_.getQuotas()) {
    if (quota.duration_ != 0 && quota.max_count_ != 0) {
      Matcher::hashUpdate(&quota.duration_, sizeof(quota.duration_), hash_code);
      Matcher::hashUpdate(&quota.max_count_, sizeof(quota.max_count_), hash_code);
    }
  }
}

bool Rule::hasUserNameMatch() const {
  for(const auto& matcher : matchers_){
    if(!matcher->getUserMatchers().empty()){
      return true;
    }
  }
  return false;
}
} // namespace Impl
} // namespace StrongGlobalRatelimit
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
