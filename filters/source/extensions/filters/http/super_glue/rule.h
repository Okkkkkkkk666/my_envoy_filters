#pragma once

#include "filters/api/envoy/extensions/filters/http/super_glue/v3/super_glue_client.pb.h"

#include "filters/source/extensions/filters/http/common/strong_matcher/matcher.h"
#include "filters/source/extensions/filters/http/common/strong_matcher/rule.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SuperGlueFilter {

namespace v3 = envoy::extensions::filters::http::super_glue::v3;

class Rule {
public:
  Rule(const v3::Rule& rule);

  const std::string& ruleName() const {return rule_name_;}
  // 检查当前请求是否与本规则匹配.
  bool match(const std::string& username, const Network::Address::InstanceConstSharedPtr& address,
             const Http::RequestHeaderMap& headers) const;
  bool hasUsernameMatch() const;

private:
  const std::string rule_name_;
  const std::vector<std::unique_ptr<Filters::Common::StrongMatcher::Matcher>> matchers_;
};
using RulePtr = std::unique_ptr<Rule>;
} // namespace SuperGlueFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
