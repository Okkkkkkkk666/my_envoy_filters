#include <algorithm>

#include "matcher.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace StrongMatcher {

Matcher::Matcher(const v3::Matcher& matcher)
    : path_matcher_(makePathMatcher(matcher)),
      header_matchers_(Http::HeaderUtility::buildHeaderDataVector(matcher.headers())),
      query_parameter_matchers_(buildQueryParameterMatcherVector(matcher)),
      methods_(buildMethodsVector(matcher)), user_matchers_(buildUserMatcherVector(matcher)),
      ip_range_(buildIpRangeVector(matcher)), ip_list_(buildIpSet(matcher)),
      method_invert_(matcher.method_invert()), ip_invert_(matcher.ip_list().invert()),
      path_invert_(matcher.path_invert()), hash_code_(calcHash(matcher)) {}

Matchers::PathMatcherConstSharedPtr Matcher::makePathMatcher(const v3::Matcher& matcher) const {
  Matchers::PathMatcherConstSharedPtr matcher_ptr = nullptr;
  switch (matcher.path_specifier_case()) {
  case v3::Matcher::kPrefix:
    matcher_ptr = Matchers::PathMatcher::createPrefix(matcher.prefix(), !matcher.case_sensitive());
    break;
  case v3::Matcher::kExact:
    matcher_ptr = Matchers::PathMatcher::createExact(matcher.exact(), !matcher.case_sensitive());
    break;
  case v3::Matcher::kContains:
    matcher_ptr = createContainsPath(matcher.contains(), !matcher.case_sensitive());
    break;
  case v3::Matcher::kRegex: {
    envoy::type::matcher::v3::RegexMatcher regex_matcher;
    regex_matcher.mutable_google_re2();
    regex_matcher.set_regex(matcher.regex());
    matcher_ptr = Matchers::PathMatcher::createSafeRegex(regex_matcher);
  } break;
  default:
    break;
  }

  return matcher_ptr;
}

Matchers::PathMatcherConstSharedPtr Matcher::createContainsPath(const std::string& contains,
                                                                bool ignore_case) {
  envoy::type::matcher::v3::StringMatcher matcher;
  matcher.set_contains(contains);
  matcher.set_ignore_case(ignore_case);
  return std::make_shared<const Matchers::PathMatcher>(matcher);
}

std::vector<QueryParameterMatcherPtr>
Matcher::buildQueryParameterMatcherVector(const v3::Matcher& matcher) const {
  std::vector<QueryParameterMatcherPtr> matchers;
  for (const auto& query_parameter : matcher.query_parameters()) {
    matchers.emplace_back(std::make_unique<QueryParameterMatcher>(query_parameter));
  }
  return matchers;
}

std::vector<std::string> Matcher::buildMethodsVector(const v3::Matcher& matcher) const {
  std::vector<std::string> methods;
  for (const auto& method : matcher.methods()) {
    methods.emplace_back(method);
    std::transform(methods.back().begin(), methods.back().end(), methods.back().begin(), ::toupper);
  }

  return methods;
}

std::vector<UserMatcherptr> Matcher::buildUserMatcherVector(const v3::Matcher& matcher) const {
  std::vector<UserMatcherptr> matchers;
  for (const auto& user_matcher : matcher.users()) {
    matchers.emplace_back(std::make_unique<UserMatcher>(user_matcher));
  }

  return matchers;
}

std::vector<IpRangePtr> Matcher::buildIpRangeVector(const v3::Matcher& matcher) const {
  std::vector<IpRangePtr> matchers;
  for (const auto& ip_range : matcher.ip_list().ip_range()) {
    matchers.emplace_back(std::make_unique<IpRange>(ip_range));
  }
  return matchers;
}

IpSetPtr Matcher::buildIpSet(const v3::Matcher& matcher) const {
  if (matcher.ip_list().has_ip_set()) {
    return std::make_unique<IpSet>(matcher.ip_list().ip_set());
  }
  return nullptr;
}

uint64_t Matcher::calcHash(const v3::Matcher& matcher) {
  uint64_t hash_code = 0;

  updateHashWithPath(matcher, hash_code);
  updateHashWithHeader(matcher, hash_code);
  updateHashWithQureyParameter(matcher, hash_code);
  updateHashWithMethods(matcher, hash_code);
  updateHashWithUserMatcher(matcher, hash_code);
  updateHashWithIpRange(matcher, hash_code);
  updateHashWithIpSet(matcher, hash_code);
  return hash_code;
}

void Matcher::updateHashWithPath(const v3::Matcher& matcher, uint64_t& hash_code) {
  v3::Matcher::PathSpecifierCase path_specifier_case = matcher.path_specifier_case();
  hashUpdate(&path_specifier_case, sizeof(path_specifier_case), hash_code);
  switch (path_specifier_case) {
  case v3::Matcher::PathSpecifierCase::kPrefix:
    hashUpdate(matcher.prefix().c_str(), matcher.prefix().size(), hash_code);
    break;
  case v3::Matcher::PathSpecifierCase::kExact:
    hashUpdate(matcher.exact().c_str(), matcher.exact().size(), hash_code);
    break;
  case v3::Matcher::PathSpecifierCase::kRegex:
    hashUpdate(matcher.regex().c_str(), matcher.regex().size(), hash_code);
    break;
  case v3::Matcher::PathSpecifierCase::kContains:
    hashUpdate(matcher.contains().c_str(), matcher.contains().size(), hash_code);
    break;
  default:
    break;
  }
}

void Matcher::updateHashWithHeader(const v3::Matcher& matcher, uint64_t& hash_code) {
  for (const auto& header_matcher : matcher.headers()) {
    hashUpdate(header_matcher.name().c_str(), header_matcher.name().size(), hash_code);
    hashUpdate(header_matcher.header_match_specifier_case(), hash_code);
    switch (header_matcher.header_match_specifier_case()) {
    case envoy::config::route::v3::HeaderMatcher::HeaderMatchSpecifierCase::kExactMatch:
      hashUpdate(header_matcher.exact_match().c_str(), header_matcher.exact_match().size(),
                 hash_code);
      break;
    case envoy::config::route::v3::HeaderMatcher::HeaderMatchSpecifierCase::kSafeRegexMatch:
      hashUpdate(header_matcher.safe_regex_match().regex().c_str(),
                 header_matcher.safe_regex_match().regex().size(), hash_code);
      break;
    case envoy::config::route::v3::HeaderMatcher::HeaderMatchSpecifierCase::kRangeMatch:
      hashUpdate(header_matcher.range_match().start(), hash_code);
      hashUpdate(header_matcher.range_match().end(), hash_code);
      break;
    case envoy::config::route::v3::HeaderMatcher::HeaderMatchSpecifierCase::kPresentMatch:
      hashUpdate(header_matcher.present_match(), hash_code);
      break;
    case envoy::config::route::v3::HeaderMatcher::HeaderMatchSpecifierCase::kPrefixMatch:
      hashUpdate(header_matcher.prefix_match().c_str(), header_matcher.prefix_match().size(),
                 hash_code);
      break;
    case envoy::config::route::v3::HeaderMatcher::HeaderMatchSpecifierCase::kSuffixMatch:
      hashUpdate(header_matcher.suffix_match().c_str(), header_matcher.suffix_match().size(),
                 hash_code);
      break;
    case envoy::config::route::v3::HeaderMatcher::HeaderMatchSpecifierCase::kContainsMatch:
      hashUpdate(header_matcher.contains_match().c_str(), header_matcher.contains_match().size(),
                 hash_code);
      break;
    case envoy::config::route::v3::HeaderMatcher::HeaderMatchSpecifierCase::kStringMatch:
      updateHashWithStringMatcher(header_matcher.string_match(), hash_code);
      break;
    default:
      break;
    }
  }
}

void Matcher::updateHashWithQureyParameter(const v3::Matcher& matcher, uint64_t& hash_code) {
  for (const auto& query_parameter : matcher.query_parameters()) {
    hashUpdate(query_parameter.name().c_str(), query_parameter.name().size(), hash_code);
    hashUpdate(query_parameter.value().case_sensitive(), hash_code);
    hashUpdate(query_parameter.value().invert(), hash_code);
    if (query_parameter.value().has_present()) {
      hashUpdate(query_parameter.value().present(), hash_code);
    }
    if (query_parameter.value().has_contains()) {
      hashUpdate(query_parameter.value().contains().c_str(),
                 query_parameter.value().contains().size(), hash_code);
    }
    if (query_parameter.value().has_exact()) {
      hashUpdate(query_parameter.value().exact().c_str(), query_parameter.value().exact().size(),
                 hash_code);
    }
    if (query_parameter.value().has_regex()) {
      hashUpdate(query_parameter.value().regex().c_str(), query_parameter.value().exact().size(),
                 hash_code);
    }
  }
}

void Matcher::updateHashWithMethods(const v3::Matcher& matcher, uint64_t& hash_code) {
  hashUpdate(matcher.method_invert(), hash_code);
  for (const auto& method : matcher.methods()) {
    hashUpdate(method.c_str(), method.size(), hash_code);
  }
}

void Matcher::updateHashWithStringMatcher(const envoy::type::matcher::v3::StringMatcher& matcher,
                                          uint64_t& hash_code) {
  hashUpdate(matcher.match_pattern_case(), hash_code);
  switch (matcher.match_pattern_case()) {
  case envoy::type::matcher::v3::StringMatcher::MatchPatternCase::kExact:
    hashUpdate(matcher.exact().c_str(), matcher.exact().size(), hash_code);
    break;
  case envoy::type::matcher::v3::StringMatcher::MatchPatternCase::kPrefix:
    hashUpdate(matcher.prefix().c_str(), matcher.prefix().size(), hash_code);
    break;
  case envoy::type::matcher::v3::StringMatcher::MatchPatternCase::kSuffix:
    hashUpdate(matcher.suffix().c_str(), matcher.suffix().size(), hash_code);
    break;
  case envoy::type::matcher::v3::StringMatcher::MatchPatternCase::kSafeRegex:
    hashUpdate(matcher.safe_regex().regex().c_str(), matcher.safe_regex().regex().size(),
               hash_code);
    break;
  case envoy::type::matcher::v3::StringMatcher::MatchPatternCase::kContains:
    hashUpdate(matcher.contains().c_str(), matcher.contains().size(), hash_code);
    break;
  default:
    break;
  }
  hashUpdate(matcher.ignore_case(), hash_code);
}

void Matcher::updateHashWithUserMatcher(const v3::Matcher& matcher, uint64_t& hash_code) {
  for (const auto& user_matcher : matcher.users()) {
    hashUpdate(user_matcher.user_name().c_str(), hash_code);
    hashUpdate(user_matcher.case_sensitive(), hash_code);
    hashUpdate(user_matcher.invert(), hash_code);
  }
}
void Matcher::updateHashWithIpSet(const v3::Matcher& matcher, uint64_t& hash_code) {
  Matcher::hashUpdate(matcher.ip_list().ip_set().name().c_str(), matcher.ip_list().ip_set().name().size(),
                      hash_code);
  Matcher::hashUpdate(matcher.ip_list().ip_set().desc().c_str(), matcher.ip_list().ip_set().desc().size(),
                      hash_code);
  for (const auto& cidr_range : matcher.ip_list().ip_set().list()) {
    Matcher::hashUpdate(cidr_range.address_prefix().c_str(), cidr_range.address_prefix().size(),
                        hash_code);
    Matcher::hashUpdate(cidr_range.prefix_len().value(), hash_code);
  }
}

void Matcher::updateHashWithIpRange(const v3::Matcher& matcher, uint64_t& hash_code) {
  for (const auto& ip_range : matcher.ip_list().ip_range()) {
    hashUpdate(ip_range.start_ip().c_str(), ip_range.start_ip().size(), hash_code);
    hashUpdate(ip_range.end_ip().c_str(), ip_range.end_ip().size(), hash_code);
  }
}

} // namespace StrongMatcher
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
