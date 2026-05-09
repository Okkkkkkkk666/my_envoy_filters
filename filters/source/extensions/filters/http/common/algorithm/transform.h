#pragma once
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <ctime>
#include <regex>
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
class TransformRewrite : public Logger::Loggable<Logger::Id::filter> {
public:
  TransformRewrite(const v3::TransformRewrite& transform);

public:
  std::string transForm(const std::string& data, RegexMatcher::EncodingType& encode_type) const;

private:
  const v3::TransformRewrite_TransformType transform_type_;
  const uint32_t decimal_places_;
  const v3::TransformRewrite_CharacterShift_ShiftDirection direction_;
  uint32_t shift_amount_;
  const v3::TransformRewrite_DateRound_DateRoundLevel level_;
};
using TransformRewritePtr = std::unique_ptr<TransformRewrite>;
} // namespace Algorithm
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
