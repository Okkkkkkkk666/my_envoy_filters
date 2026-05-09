#include "shuffle.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Algorithm {

ShuffleRewrite::ShuffleRewrite(const v3::ShuffleRewrite& shuffle) { (void)shuffle; }

std::string ShuffleRewrite::shuffle(const std::string& data,
                                    RegexMatcher::EncodingType& encode_type) const {
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
  cpp_rule.method = static_cast<Replaces::rewrite_method>(6);
  Replaces::Rewrite rule(cpp_rule);
  return rule.rewrite(change_data);
}

} // namespace Algorithm
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy