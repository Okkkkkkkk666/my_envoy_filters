#include "regex_replacer_impl.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Regex {

RegexReplacerImpl::RegexReplacerImpl(bool ignore_case, const std::string& match_expr,
                                     const std::string& replace_expr, EncodingType encode)
    : ignore_case_(ignore_case), orig_match_expr_(match_expr), replace_expr_(replace_expr) {
  for (int i = 0; i < EncodingType::MAX; i++) {
    std::string new_pattern = RegexUtilities::EncodingConvert(encode, static_cast<EncodingType>(i), orig_match_expr_);
    if (!new_pattern.empty()) {
      match_exprs_[i] = new_pattern;
      match_re2s_[i] =
          RegexUtilities::EncodingRe(ignore_case_, new_pattern, static_cast<EncodingType>(i));
      if (!match_re2s_[i]) {
        ENVOY_LOG(error, "Encoding re2 regex failed: {}", orig_match_expr_);
      }
    } else {
      ENVOY_LOG(error, "Encoding convert failed: {}", orig_match_expr_);
    }
  }
}

bool RegexReplacerImpl::replace(std::string& in, EncodingType code) {
  if (code >= EncodingType::MAX || !match_re2s_[code]) {
    return false;
  }
  return re2::RE2::Replace(&in, *match_re2s_[code], replace_expr_);
}

} // namespace Regex
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework