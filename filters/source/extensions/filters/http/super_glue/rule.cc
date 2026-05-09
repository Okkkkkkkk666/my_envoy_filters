#include "rule.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SuperGlueFilter {
Rule::Rule(const v3::Rule& rule)
    : rule_name_(rule.rule_name()), matchers_([&rule]() {
        std::vector<std::unique_ptr<Filters::Common::StrongMatcher::Matcher>> matchers;
        for (const auto& matcher : rule.matchers()) {
          matchers.emplace_back(std::make_unique<Filters::Common::StrongMatcher::Matcher>(matcher));
        }
        return matchers;
      }()) {}
bool Rule::match(const std::string& username,
                 const Network::Address::InstanceConstSharedPtr& address,
                 const Http::RequestHeaderMap& headers) const {
  for (const auto& matcher : matchers_) {
    if (!matcher->matchAll(headers, address, username)) {
      return false;
    }
  }
  return true;
}

bool Rule::hasUsernameMatch() const {
  for(const auto& matcher : matchers_){
    if(!matcher->getUserMatchers().empty()){
      return true;
    }
  }
  return false;
}

} // namespace SuperGlueFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy