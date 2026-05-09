#pragma once

#include <memory>
#include <string>
#include <vector>

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace PublicLib {

class PublicLibCache {
protected:
  virtual ~PublicLibCache() = default;

public:
  /**
   * 获取规则数据
   * @param policy_id 策略包id
   * @param rule_id 规则id
   * @return 规则数据
   */
  virtual std::string getRule(const std::string& policy_id, const std::string& rule_id) = 0;
};

using PublicLibCacheSharedPtr = std::shared_ptr<PublicLibCache>;
} // namespace PublicLib
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework