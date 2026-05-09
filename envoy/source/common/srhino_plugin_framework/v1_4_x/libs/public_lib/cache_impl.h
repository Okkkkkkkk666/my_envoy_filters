#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "envoy/srhino_plugin_framework/v1_4_x/libs/public_lib/cache.h"
#include "envoy/srhino_plugin_framework/v1_4_x/utility/singleton.hpp"
#include "envoy/srhino_plugin_framework/v1_4_x/utility/hash_table.hpp"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace PublicLib {

class PublicLibCacheImpl : public PublicLibCache {
public:
  PublicLibCacheImpl() = default;

public:
  /**
   * 获取规则数据
   * @param policy_id 策略包id
   * @param rule_id 规则id
   * @return 规则数据
   */
  std::string getRule(const std::string& policy_id, const std::string& rule_id) override;

  /**
   * 插入
   * @param policy_id 策略包id
   * @param rule_id 规则id
   * @param rule_data 规则数据
   */
  void insertRule(const std::string& policy_id, const std::string& rule_id,
                  const std::string& rule_data);

  /**
   * 更新，不存在则会执行插入
   * @param policy_id 策略包id
   * @param rule_id 规则id
   * @param rule_data 规则数据
   */
  void updateRule(const std::string& policy_id, const std::string& rule_id,
                  const std::string& rule_data);

  /**
   * 删除规则
   * @param policy_id 策略包id
   * @param rule_id 规则id
   */
  void delRule(const std::string& policy_id, const std::string& rule_id);

  /**
   * 删除策略
   * @param policy_id 策略包id
   */
  void delPolicy(const std::string& policy_id);

private:
  using RuleTable = std::unordered_map<std::string /*规则id*/, std::string /*规则数据*/>;
  using RuleTableSharedPtr = std::shared_ptr<RuleTable>;
  using Cache =
      Utility::Singleton<Utility::HashTable<std::string /*策略包id*/, RuleTableSharedPtr>>;
};
using PublicLibCacheImplSharedPtr = std::shared_ptr<PublicLibCacheImpl>;

} // namespace PublicLib
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework