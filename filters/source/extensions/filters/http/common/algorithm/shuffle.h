#pragma once
#include <iostream>
#include <algorithm>
#include <vector>
#include <random>
#include <codecvt>
#include <locale>
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
class ShuffleRewrite : public Logger::Loggable<Logger::Id::filter> {
public:
  ShuffleRewrite(const v3::ShuffleRewrite& shuffle);

public:
  std::string shuffle(const std::string& data, RegexMatcher::EncodingType& encode_type) const;
};

using ShuffleRewritePtr = std::shared_ptr<ShuffleRewrite>;
} // namespace Algorithm
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy