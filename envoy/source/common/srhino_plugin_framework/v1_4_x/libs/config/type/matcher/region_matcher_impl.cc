#include "region_matcher_impl.h"
namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {

RegionMatcherImpl::RegionMatcherImpl(
    const srhino_plugin_framework::v1_4_x::proto::config::type::matcher::RegionMatcher& matcher)
    : invert_(matcher.invert()), region_name_(matcher.name().begin(), matcher.name().end()) {}

bool RegionMatcherImpl::match(const std::string& name) const {
  bool is_match = false;
  std::vector<std::string> name_parts;
  std::istringstream ss(name);
  std::string part;
  while (std::getline(ss, part, '-')) {
    name_parts.push_back(part);
  }

  if (name_parts.empty()) {
    return false;
  }

  // 判断是否为内网ip
  if (name_parts[0].find(intranet_work_code_) != std::string::npos) {
    return false;
  }

  auto get_code_after_colon = [](const std::string& str) {
    auto pos = str.find(':');
    return pos == std::string::npos ? str : str.substr(pos + 1);
  };

  // 提取国家code
  std::string country_code = get_code_after_colon(name_parts[0]);

  // 国内ip匹配
  if (name_parts[0].find(china_region_code_) != std::string::npos) {
    // 检查国家代码是否匹配
    bool country_match = std::find(region_name_.begin(), region_name_.end(), country_code) != region_name_.end();
    
    if (name_parts.size() > 1) {
      std::string province_code = get_code_after_colon(name_parts[1]);
      bool province_match = std::find(region_name_.begin(), region_name_.end(), province_code) != region_name_.end();
      is_match = country_match || province_match; 
    } else {
      is_match = country_match;
    }

  } else {
    //国外ip匹配
    is_match = std::find(region_name_.begin(), region_name_.end(), country_code) != region_name_.end();
  }
  return invert_ ? !is_match : is_match;
}

} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework
