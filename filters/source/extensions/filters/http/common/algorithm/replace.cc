#include "replace.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Algorithm {
ReplaceRewrite::ReplaceRewrite(const v3::ReplaceRewrite& replace)
    : replace_mode_(replace.rule_type()), replace_value_(replace.replace_value()),
      sample_value_(replace.sample_value().sample_content()), value_type_(replace.value_type()),
      regex_(replace.regex_match().regex()), cover_type_(replace.custom().cover_type()) {
  switch (replace_mode_) {
  case v3::ReplaceRewrite::CUSTOM:
    for (const auto& rule : replace.custom().rules()) {
      rules_.push_back(rule);
    }
    break;
  default:
    break;
  }
}

std::string ReplaceRewrite::execudata(const std::string& data,
                                      RegexMatcher::EncodingType& encode_type) const {
  std::string result;
  std::string change_data = data;
  RegexMatcher::RegexUtilities regex_utilit;
  std::string replace_content;
  if (encode_type == RegexMatcher::GBK) {
    if (!regex_utilit.convertImpl(data, change_data, "GBK", "UTF-8")) {
      ENVOY_LOG(error, "covert encoding gbk failed: {}");
      return data;
    }
  } else if (encode_type == RegexMatcher::ISO) {
    if (!regex_utilit.convertImpl(data, change_data, "ISO-8859-1", "UTF-8")) {
      ENVOY_LOG(error, "covert encoding ISO failed: {}");
      return data;
    }
  }
  Replaces::rewrite_rule cpp_rule;
  cpp_rule.method = static_cast<Replaces::rewrite_method>(4);
  cpp_rule.replace.value_type = static_cast<Replaces::ValueType>(value_type_);
  if (value_type_ == v3::ReplaceRewrite::ValueType::ReplaceRewrite_ValueType_SAMPLE_VALUE) {
    replace_content = sample_value_;
  } else {
    replace_content = replace_value_;
  }
  if (replace_mode_ == v3::ReplaceRewrite::REGEX_MATCH) { // 正则匹配
    cpp_rule.replace.rule_type = static_cast<Replaces::RuleType>(replace_mode_);
    cpp_rule.replace.regex_match.regex = regex_;

    cpp_rule.replace.replace_value = replace_content;
  } else if (replace_mode_ == v3::ReplaceRewrite::CUSTOM) { // 自定义
    cpp_rule.replace.rule_type = static_cast<Replaces::RuleType>(replace_mode_);
    for (size_t i = 0; i < rules_.size(); ++i) {
      {
        Replaces::ReplaceRuleDefinition replace_rule;
        replace_rule.start = rules_[i].start();
        replace_rule.end = rules_[i].end();
        cpp_rule.replace.custom.rules.emplace_back(replace_rule);
      }
    }
    cpp_rule.replace.custom.cover_type = static_cast<Replaces::ReplaceCoverType>(cover_type_);
    cpp_rule.replace.replace_value = replace_content;
  } else {
    cpp_rule.replace.rule_type = static_cast<Replaces::RuleType>(replace_mode_);
    cpp_rule.replace.replace_value = replace_content;
  }
  Replaces::Rewrite rule(cpp_rule);
  return rule.rewrite(change_data);
}

} // namespace Algorithm
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy