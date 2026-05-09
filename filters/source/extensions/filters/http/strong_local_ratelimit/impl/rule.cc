#include "source/common/http/path_utility.h"

#include "rule.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {
namespace Impl {

Rule::Rule(const v3::Rule& rule)
    : matchers_([&rule]() {
        std::vector<std::unique_ptr<Filters::Common::StrongMatcher::Matcher>> result;
        for (const auto& matcher : rule.condition()) {
          result.emplace_back(std::make_unique<Filters::Common::StrongMatcher::Matcher>(matcher));
        }
        return result;
      }()),
      rule_name_(rule.rule_name()), action_(rule.action()), hash_code_(calcHash()) {}

bool Rule::match(const std::string& user_name,
                 const Network::Address::InstanceConstSharedPtr& address,
                 const Http::RequestHeaderMap& headers) const {

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

  auto proc_next_rule = action_.procNextRule();
  Matcher::hashUpdate(&proc_next_rule, sizeof(proc_next_rule), hash_code);
  for (const Action::QuotaConfig& quota : action_.quotaConfig()) {
    if (quota.duration != 0 && quota.max_count != 0) {
      Matcher::hashUpdate(&quota.duration, sizeof(quota.duration), hash_code);
      Matcher::hashUpdate(&quota.max_count, sizeof(quota.max_count), hash_code);
    }
  }
}

bool Rule::hasUserNameMatch() const {
  for (const auto& matcher : matchers_) {
    if (!matcher->getUserMatchers().empty()) {
      return true;
    }
  }
  return false;
}

} // namespace Impl
} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy