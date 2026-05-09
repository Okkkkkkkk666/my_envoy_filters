#pragma once
#include <iostream>
#include <algorithm>
#include <vector>
#include <random>
#include <locale>
#include <codecvt>
#include <iconv.h>
#include <cstring>
#include <memory>
#include <cctype>
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

class CoverRewrite : public Logger::Loggable<Logger::Id::filter>{
public:
  CoverRewrite(const v3::CoverRewrite& cover);

public:
  std::string coverData(const std::string& data, RegexMatcher::EncodingType& encode_type) const;

private:
  const std::string cover_character_;
  const v3::CoverRewrite_CoverMode cover_mode_;
  const v3::CoverType cover_type_;
  std::vector<v3::CoverRewrite_CoverRuleDefinition> rules_;
};
using CoverRewritePtr = std::unique_ptr<CoverRewrite>;
} // namespace Algorithm
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
