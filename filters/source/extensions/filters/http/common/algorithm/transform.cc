#include "transform.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Algorithm {

TransformRewrite::TransformRewrite(const v3::TransformRewrite& transform)
    : transform_type_(transform.transform_type()),
      decimal_places_(transform.numeric_round().decimal_places()),
      direction_(transform.character_shift().direction()),
      shift_amount_(transform.character_shift().shift_amount()),
      level_(transform.date_round().level()) {}

std::string TransformRewrite::transForm(const std::string& data, RegexMatcher::EncodingType& encode_type) const {
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
  cpp_rule.method = static_cast<Replaces::rewrite_method>(5);
  cpp_rule.transform.type = static_cast<Replaces::TransformType>(transform_type_);
  if (transform_type_ == v3::TransformRewrite::NUMERIC_ROUND) { // 数字取整
    cpp_rule.transform.numeric_round.decimal_places = decimal_places_;
  }
  if (transform_type_ == v3::TransformRewrite::DATE_ROUND) { // 日期取整
    cpp_rule.transform.date_round.level = static_cast<Replaces::DateRoundLevel>(level_);
  }
  if (transform_type_ == v3::TransformRewrite::CHARACTER_SHIFT) { // 字符位移
    cpp_rule.transform.character_shift.direction =
        static_cast<Replaces::ShiftDirection>(direction_);
    cpp_rule.transform.character_shift.shift_amount = shift_amount_;
  }

  Replaces::Rewrite rule(cpp_rule);
  return rule.rewrite(change_data);
}

} // namespace Algorithm
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
