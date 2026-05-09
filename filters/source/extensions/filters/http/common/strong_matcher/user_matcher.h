#pragma once

#include "source/common/router/config_utility.h"
#include <unordered_map>
#include "filters/api/envoy/extensions/filters/http/common/strong_matcher/v3/matcher.pb.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace StrongMatcher {

namespace v3 = envoy::extensions::filters::http::common::strong_matcher::v3;

class UserMatcher {
public:
  UserMatcher(const v3::UserMatcher& matcher);

public:
  bool matches(const std::string& matcher_user_identification) const;
  std::vector<std::string> splitUserNameList(const std::string& user_name) const;
  std::string toLowercase(const std::string& source_string) const;
  std::string username() const { return user_name_; }
private:
  std::string user_name_;
  bool case_sensitive_;
  bool invert_;
  std::unordered_map<std::string, int> split_user_name_;
};
using UserMatcherptr = std::unique_ptr<const UserMatcher>;

} // namespace StrongMatcher
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
