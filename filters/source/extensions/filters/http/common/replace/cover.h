#pragma once
#include <iostream>
#include <algorithm>
#include <vector>
#include <random>
#include <vector>
#include <memory>
#include "string_utils.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
enum CoverType { COVER_UNKNOWN = 0, COVER = 1, RESERVE = 2 };
// 遮盖方式
enum CoverMode {
  MODE_UNKNOWN = 0,
  COVER_ALL = 1,
  RESERVE_FIRST1_LAST1 = 2,
  RESERVE_FIRST3_LAST2 = 3,
  RESERVE_FIRST3_LAST4 = 4,
  CUSTOM = 5,
  RESERVE_LAST4 = 6
};
struct CoverRule {
  uint32_t start = 1;
  uint32_t end = 2;
};

struct Cover {
  CoverMode cover_mode;         // 遮盖方式
  std::vector<CoverRule> rules; // 自定义规则
  std::string cover_character;  // 遮盖字符
  CoverType cover_type;         // 遮盖类型
};

class CoverRuleDefinition {
public:
  CoverRuleDefinition(const CoverRule& rule);

public:
  std::string reserveLocation(const std::string& data, std::vector<bool>& status) const;

private:
  const uint32_t start_;
  const uint32_t end_;
};

class CoverRewrite {
public:
  CoverRewrite(const Cover& cover);

public:
  std::string coverData(const std::string& data) const;
  std::string coverAll(const std::string& data, const std::string& replace_value) const;
  std::string coverFirst1Last1(const std::string& data, const std::string& replace_value) const;
  std::string coverFirst3Last2(const std::string& data, const std::string& replace_value) const;
  std::string coverFirst3Last4(const std::string& data, const std::string& replace_value) const;
  std::string coverLast4(const std::string& data, const std::string& replace_value) const;
  std::string customRule(const std::string& data, const std::string& replace_value,
                         const CoverType cover_type) const;

private:
  const std::string cover_character_;
  const CoverMode cover_mode_;
  const CoverType cover_type_;
  std::vector<std::unique_ptr<CoverRuleDefinition>> rules_ptr_;
};
} // namespace Replace
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy