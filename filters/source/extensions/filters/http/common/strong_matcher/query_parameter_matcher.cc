#include "query_parameter_matcher.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace StrongMatcher {
QueryParameterMatcher::QueryParameterMatcher(const v3::QueryParameterMatcher& matcher)
    : invert_(matcher.value().invert()) {
  // 将v3::QueryParameterMatcher转换成envoy::config::route::v3::QueryParameterMatcher
  envoy::config::route::v3::QueryParameterMatcher config;
  config.set_name(matcher.name());
  const auto& value = matcher.value();
  if (value.has_present()) {
    config.set_present_match(value.present());
  } else {
    envoy::type::matcher::v3::StringMatcher* string_matcher = config.mutable_string_match();

    if (value.has_contains()) {
      string_matcher->set_ignore_case(!value.case_sensitive());
      string_matcher->set_contains(value.contains());
    }

    if (value.has_exact()) {
      string_matcher->set_ignore_case(!value.case_sensitive());
      string_matcher->set_exact(value.exact());
    }

    if (value.has_regex()) {
      auto regex = string_matcher->mutable_safe_regex();
      regex->mutable_google_re2();
      *(regex->mutable_regex()) = value.regex();
    }
  }

  matcher_ = std::make_unique<Router::ConfigUtility::QueryParameterMatcher>(config);
}

bool QueryParameterMatcher::matches(const Http::Utility::QueryParams& query_parameters) const {
  bool result = matcher_->matches(query_parameters);
  return invert_ ? !result : result;
}

} // namespace StrongMatcher
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy