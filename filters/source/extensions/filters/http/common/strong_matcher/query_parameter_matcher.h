#pragma once

#include "source/common/router/config_utility.h"

#include "filters/api/envoy/extensions/filters/http/common/strong_matcher/v3/matcher.pb.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace StrongMatcher {

namespace v3 = envoy::extensions::filters::http::common::strong_matcher::v3;

class QueryParameterMatcher {
public:
  QueryParameterMatcher(const v3::QueryParameterMatcher& matcher);

public:
  bool matches(const Http::Utility::QueryParams& query_parameters) const;

private:
  Router::ConfigUtility::QueryParameterMatcherPtr matcher_;
  bool invert_;
};

using QueryParameterMatcherPtr = std::unique_ptr<const QueryParameterMatcher>;

} // namespace StrongMatcher
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy