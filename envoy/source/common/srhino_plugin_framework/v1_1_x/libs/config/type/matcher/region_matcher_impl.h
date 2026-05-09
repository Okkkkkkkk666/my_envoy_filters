#pragma once
#include "source/common/router/config_utility.h"
#include <unordered_map>
#include "envoy/srhino_plugin_framework/v1_1_x/libs/config/type/matcher/region_matcher.h"
namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
const std::string intranet_work_code_ = "999999";
const std::string china_region_code_ = "156";
class RegionMatcherImpl : public RegionMatcher {
public:
  RegionMatcherImpl(const srhino_plugin_framework::v1_1_x::proto::config::type::matcher::RegionMatcher& matcher);

public:
  bool match(const std::string& name) const override;

private:
  bool invert_;
  std::vector<std::string> region_name_;
};
using RegionMatcherptr = std::unique_ptr<const RegionMatcherImpl>;
} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework