#include "user_matcher.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace StrongMatcher {

UserMatcher::UserMatcher(const v3::UserMatcher& matcher)
    : user_name_(matcher.user_name()), case_sensitive_(matcher.case_sensitive()),
      invert_(matcher.invert()), split_user_name_([&matcher, this]() {
        std::int32_t id = 1;
        std::unordered_map<std::string, int> username;
        auto user_name_ = splitUserNameList(toLowercase(matcher.user_name()));
        for (auto& name : user_name_) {
          username.insert(std::make_pair(name, id));
          id++;
        }
        return username;
      }()) {}

bool UserMatcher::matches(const std::string& matcher_user_identification) const {
  bool is_match = false;
  // 转小写
  auto identification = toLowercase(matcher_user_identification);

  if (!split_user_name_.empty() && !identification.empty()) {
    if (split_user_name_.find(identification) != split_user_name_.end()) {
      is_match = true;
    }
  }

  if (invert_) {
    is_match = !is_match;
  }

  return is_match;
}

std::vector<std::string> UserMatcher::splitUserNameList(const std::string& user_name) const {
  std::vector<std::string> username;
  if (!user_name.empty()) {
    std::stringstream ss(user_name);
    while (ss.good()) {
      std::string substr;
      std::getline(ss, substr, ',');
      username.push_back(substr);
    }
  }
  return username;
}

std::string UserMatcher::toLowercase(const std::string& source_string) const {
  std::string result = source_string;
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return result;
}

} // namespace StrongMatcher
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy