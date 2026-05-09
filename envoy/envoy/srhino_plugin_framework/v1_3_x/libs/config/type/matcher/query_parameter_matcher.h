#pragma once

#include "source/common/router/config_utility.h"
#include "envoy/srhino_plugin_framework/v1_3_x/proto/config/type/matcher/matcher.pb.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
class QueryParameterMatcher {
public:
  virtual ~QueryParameterMatcher() = default;
public:
  /**
   * 查询参数匹配
   * @param query_parameters 查询参数
   * @return true
   * @return false
   */
  virtual bool matches(const Envoy::Http::Utility::QueryParams& query_parameters) const = 0;
};

} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework