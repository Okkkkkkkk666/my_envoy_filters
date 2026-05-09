#pragma once

#include <string>

#include "envoy/srhino_plugin_framework/v1_4_x/proto/config/type/matcher/matcher.pb.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {

class Time {
public:
  virtual ~Time() = default;

public:
  /**
   * 判断时间是否匹配
   * @return true
   * @return false
   */
  virtual bool match() const = 0;
};

class DateMatcher {
public:
  virtual ~DateMatcher() = default;

public:
  /**
   * 判断时间是否匹配
   * @return true
   * @return false
   */
  virtual bool match() const = 0;
};

} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework