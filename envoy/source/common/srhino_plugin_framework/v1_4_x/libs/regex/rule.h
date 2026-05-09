#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {

// 约束
struct ConstraintRule {
  // 正则表达式
  std::string expr;
  // 是否取反
  bool invert{false};
  // 是否匹配空串（如果开头或结尾没有数据，是否匹配）
  bool match_null{false};
  // 最大扫描长度（开头）
  uint32_t max_match_length{5};
};

// 单条数据标签规则
struct RuleConfig {
  // 规则id 注意，同一模式集内的ID必须唯一
  uint64_t id{0};
  // 规则名称，保留
  std::string name;
  // 规则描述，保留
  std::string description;
  // 数据标签规则 & 正则表达式
  std::string expr;
  // 是否忽略大小写
  bool case_less{false};
  // 是否开启left_most多模扫描，默认false不开启
  // 开启条件：主规则匹配可能超过128字节&&(有before约束 || 有contain约束 ||需要定位from)
  bool left_most{false};
  // 包含约束
  ConstraintRule contain;
  // 开头约束
  ConstraintRule before;
  // 末尾约束
  ConstraintRule after;
};
using RuleConfigs = std::vector<RuleConfig>;

class RuleConfigList {
public:
  RuleConfigList() = default;
  ~RuleConfigList() = default;

  const RuleConfigs& rules() const { return rules_; }

protected:
  RuleConfigs rules_;
};

} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework