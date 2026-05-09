#pragma once
#include <iostream>
#include <algorithm>
#include <vector>
#include <random>
#include <regex>
#include <sstream>
#include <iterator>
#include "source/common/common/logger.h"
#include "filters/api/envoy/extensions/filters/http/common/algorithm/v3/algorithm.pb.h"
#include "filters/source/extensions/filters/http/common/regex_matcher/regex_matcher.h"
#include "filters/source/extensions/filters/http/common/replace/rewrite_rule.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Algorithm {
namespace v3 = envoy::extensions::filters::http::common::algorithm::v3;
namespace RegexMatcher = Envoy::Extensions::Filters::Common::RegexMatcher;
namespace Replaces = Envoy::Extensions::Filters::Common::Replaces;

class ReplaceRewrite : public Logger::Loggable<Logger::Id::filter> {
public:
  ReplaceRewrite(const v3::ReplaceRewrite& replace);

public:
  std::string execudata(const std::string& data, RegexMatcher::EncodingType& encode_type) const;

private:
  const v3::ReplaceRewrite_RuleType replace_mode_; // 替换方式
  const std::string replace_value_;
  const std::string sample_value_;
  const v3::ReplaceRewrite::ValueType value_type_;
  std::vector<v3::ReplaceRewrite_ReplaceRuleDefinition> rules_;
  const std::string regex_;
  const v3::CoverType cover_type_;
};
using ReplaceRewritePtr = std::unique_ptr<ReplaceRewrite>;
} // namespace Algorithm
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy