#pragma once
#include "source/common/router/config_utility.h"
#include <unordered_map>
#include "envoy/srhino_plugin_framework/v1_3_x/libs/config/type/matcher/user_matcher.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {

class UserMatcherImpl : public UserMatcher {
public:
  UserMatcherImpl(
      const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::UserMatcher& matcher);

public:
  bool matches(const std::string& matcher_user_identification) const override;
  std::vector<std::string> splitUserNameList(const std::string& user_name) const;
  std::string toLowercase(const std::string& source_string) const;
  std::string username() const { return user_name_; }

private:
  std::string user_name_;
  bool invert_;
  std::unordered_map<std::string, int> split_user_name_;
};
using UserMatcherptr = std::unique_ptr<const UserMatcherImpl>;

} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework
