#include "cover.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Algorithm {
namespace v3 = envoy::extensions::filters::http::common::algorithm::v3;
CoverRewrite::CoverRewrite(const v3::CoverRewrite& cover)
    : cover_character_(cover.cover_character()), cover_mode_(cover.cover_mode()),
      cover_type_(cover.cover_type()) {
  switch (cover_mode_) {
  case v3::CoverRewrite::CoverMode::CoverRewrite_CoverMode_CUSTOM:
    for (const auto& rule : cover.rules()) {
      rules_.push_back(rule);
    }
    break;
  default:
    break;
  }
}

std::string CoverRewrite::coverData(const std::string& data,
                                    RegexMatcher::EncodingType& encode_type) const {
  std::string result;
  std::string change_data = data;
  RegexMatcher::RegexUtilities regex_utilit;

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
  cpp_rule.method = static_cast<Replaces::rewrite_method>(3);
  if (cover_mode_ == v3::CoverRewrite::CoverMode::CoverRewrite_CoverMode_CUSTOM) {
    cpp_rule.cover.cover_mode = static_cast<Replaces::CoverMode>(cover_mode_);
    cpp_rule.cover.cover_type = static_cast<Replaces::CoverType>(cover_type_);
    for (size_t i = 0; i < rules_.size(); ++i) {
      Replaces::CoverRule cover_rule;
      cover_rule.start = rules_[i].start();
      cover_rule.end = rules_[i].end();
      cpp_rule.cover.rules.emplace_back(cover_rule);
    }
    cpp_rule.cover.cover_character = cover_character_;
  } else {
    cpp_rule.cover.cover_character = cover_character_;
    cpp_rule.cover.cover_mode = static_cast<Replaces::CoverMode>(cover_mode_);
  }
  Replaces::Rewrite rule(cpp_rule);
  return rule.rewrite(change_data);
}

} // namespace Algorithm
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
