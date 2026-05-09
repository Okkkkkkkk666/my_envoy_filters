#pragma once
#include <iostream>
#include <algorithm>
#include <vector>
#include <random>
#include <regex>
#include <sstream>
#include <iterator>
#include "string_utils.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
enum ReplaceCoverType { REPLACE_COVER_UNKNOWN = 0, REPLACE_COVER = 1, REPLACE_RESERVE = 2 };
enum ValueType { VALUE_UNKNOWN = 0, FIXED_VALUE = 1, RANDOM_VALUE = 2, SAMPLE_VALUE = 3 };
enum RuleType {
  RULE_UNKNOWN = 0,
  REPLACE_ALL = 1,
  REPLACE_FIRST3 = 2,
  REPLACE_LAST4 = 3,
  REGEX_MATCH = 4,
  REPLACE_CUSTOM = 5
};
// 规则定义
struct ReplaceRuleDefinition {
  // 位数范围
  uint32_t start;
  uint32_t end;
};
// 固定值替换规则的定义
struct FixedReplace {};
// 正则匹配替换规则的定义
struct RegexMatch {
  std::string regex;
};
// 自定义替换规则的定义
struct CustomRule {
  std::vector<ReplaceRuleDefinition> rules; // 规则列表
  ReplaceCoverType cover_type;
};

struct Replace {
  RuleType rule_type;
  // 三种固定替换
  FixedReplace replace;
  // 正则匹配
  RegexMatch regex_match;
  // 自定义
  CustomRule custom;
  std::string replace_value; // 替换值
  ValueType value_type;
};

struct replacePostion {
  uint32_t start;
  uint32_t end;
};

class FixedReplaceRewrite {
public:
  FixedReplaceRewrite(const FixedReplace& replace);

public:
  std::string replaceAll(const std::string& data, const std::string& replace_value,
                         const ValueType value_type) const;
  std::string replaceFirst3(const std::string& data, const std::string& replace_value,
                            const ValueType value_type) const;
  std::string replaceLast4(const std::string& data, const std::string& replace_value,
                           const ValueType value_type) const;
};

class RegexMatchRewrite {
public:
  RegexMatchRewrite(const RegexMatch& regex);

public:
  std::string regexMatch(const std::string& data, const std::string& replace_value,
                         const ValueType value_type) const;

private:
  const std::string pattern_;
};

class ReplaceRuleDefinitionRewrite {
public:
  ReplaceRuleDefinitionRewrite(const ReplaceRuleDefinition& rule);

public:
  std::string reserveRule(const std::string& data, std::vector<replacePostion>& reserved_postion) const;
  std::string coverRule(const std::string& data, std::vector<replacePostion>& reserved_postion) const;

private:
  const uint32_t start_;
  const uint32_t end_;
  const std::vector<replacePostion> postions_;
};

class CustomRuleRewrite {
public:
  CustomRuleRewrite(const CustomRule& custom);

public:
  std::string customData(const std::string& data, const std::string& value,
                        const ValueType& value_type) const;

private:
  const std::vector<std::unique_ptr<ReplaceRuleDefinitionRewrite>> rulesPtr_;
  const ReplaceCoverType cover_type_;
};

class ReplaceRewrite {
public:
  ReplaceRewrite(const Replace& replace);

public:
  std::string execudata(const std::string& data) const;

private:
  const uint32_t replace_mode_; // 替换方式
  const std::string replace_value_;
  ReplaceCoverType cover_type_;
  const ValueType value_type_;
  std::unique_ptr<FixedReplaceRewrite> replacePtr_;
  std::unique_ptr<RegexMatchRewrite> regexPtr_;
  std::unique_ptr<CustomRuleRewrite> customPtr_;
};
} // namespace Replaces
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy