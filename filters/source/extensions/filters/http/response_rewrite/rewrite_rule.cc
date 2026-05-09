#include "rewrite_rule.h"
#include "source/common/common/assert.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ResponseRewrite {

RewriteRule::RewriteRule(const v3::RewriteRule& rule)
    : replace_utf_(rule.replace()), replace_gbk_(), rule_enable_(rule.rule_enable()),
      chinese_name_(rule.chinese_name()), regexes_() {
  EncodingConverter::EncodingRe::convertToGBK(rule.replace(), replace_gbk_);
  re2::RE2::Options options;
  options.set_case_sensitive(rule.case_less());
  regexes_.reset(new EncodingConverter::EncodingRe(rule.pattern(), options));
}

bool RewriteRule::rewrite(std::string& data, const EncodingConverter::EncodingType code) const {
  bool res = false;
  if (rule_enable_) {
    auto re = regexes_->getEncodeRegex(code);
    if (!re) {
      return res;
    }
    if (code == EncodingConverter::EncodingType::UTF8) {
      if (re2::RE2::Replace(&data, *re, replace_utf_)) {
        res = true;
      }
    } else if (code == EncodingConverter::EncodingType::GBK) {
      if (re2::RE2::Replace(&data, *re, replace_gbk_)) {
        res = true;
      }
    }
  }
  return res;
}

} // namespace ResponseRewrite
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
