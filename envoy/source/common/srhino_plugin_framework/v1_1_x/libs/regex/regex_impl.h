#pragma once

#include "envoy/srhino_plugin_framework/v1_1_x/libs/regex/regex.h"
#include "regex_matcher_impl.h"
#include "regex_replacer_impl.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Regex {

class RegexImpl : public Regex {
public:
  RegexImpl(){};
  ~RegexImpl() = default;

public:
  RegexMatcherSharedPtr createRegexMatcher(RegexMode mode, bool care_position = false,
                                           const std::string& db_path = std::string()) override;
  RegexReplacerSharedPtr createRegexReplacer(bool ignore_case, const std::string& match_expr,
                                             const std::string& replace_expr,
                                             EncodingType type) override;
};

} // namespace Regex
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework