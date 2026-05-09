#pragma once

#include <string>

#include "envoy/srhino_plugin_framework/v1_4_x/proto/config/type/matcher/matcher.pb.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {

class UserMatcher {
public:
  virtual ~UserMatcher() = default;

public:
  /**
   * 判断用户名是否匹配
   * @param username 用户名
   * @return true
   * @return false
   */
  virtual bool matches(const std::string& username) const = 0;
};

} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework