#pragma once

#include "envoy/srhino_plugin_framework/v1_3_x/proto/config/type/matcher/matcher.pb.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
class RegionMatcher {
public:
  virtual ~RegionMatcher() = default;

public:
  /**
   * 判断地址是否匹配
   * @param name 地址名
   * @return true
   * @return false
   */
  virtual bool match(const std::string& name) const = 0;
};

} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework