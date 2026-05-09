#include "source/common/srhino_plugin_framework/v1_1_x/libs/regex/regex_impl.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Regex {

RegexMatcherSharedPtr RegexImpl::createRegexMatcher(RegexMode mode, bool care_position,
                                                    const std::string& db_path) {
  return std::make_shared<RegexMatcherImpl>(mode, care_position, db_path);
}

RegexReplacerSharedPtr RegexImpl::createRegexReplacer(bool ignore_case,
                                                      const std::string& match_expr,
                                                      const std::string& replace_expr,
                                                      EncodingType type) {
  return std::make_shared<RegexReplacerImpl>(ignore_case, match_expr, replace_expr, type);
}

} // namespace Regex
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework