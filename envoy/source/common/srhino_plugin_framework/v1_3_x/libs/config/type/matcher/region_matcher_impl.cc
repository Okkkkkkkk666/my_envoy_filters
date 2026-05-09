#include "region_matcher_impl.h"
namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {

RegionMatcherImpl::RegionMatcherImpl(
    const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::RegionMatcher& matcher)
    : invert_(matcher.invert()), region_name_(matcher.name().begin(), matcher.name().end()) {}

bool RegionMatcherImpl::match(const std::string& name) const {
  bool is_match = false;
  std::vector<std::string> name_parts;
  std::istringstream ss(name);
  std::string part;
  while (std::getline(ss, part, '-')) {
    name_parts.push_back(part);
  }
  // 判断是否为内网ip
  if (name_parts[0].find(intranet_work_code_) != std::string::npos) {
    return false;
  }
  // 国内ip匹配
  if (name_parts[0].find(china_region_code_) != std::string::npos) {
    const std::string& province = name_parts[1];
    is_match = std::find(region_name_.begin(), region_name_.end(), province.substr(province.find(':') + 1)) != region_name_.end();
  } else {
    // 国外ip匹配
    const std::string& country = name_parts[0];
    is_match =
        std::find(region_name_.begin(), region_name_.end(), country.substr(country.find(':') + 1)) != region_name_.end();
  }
  return invert_ ? !is_match : is_match;
}

} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework
