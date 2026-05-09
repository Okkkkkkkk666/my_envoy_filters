#pragma once
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <ctime>
#include <regex>
#include "string_utils.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
enum TransformType { TRANS_UNKNOWN = 0, NUMERIC_ROUND = 1, DATE_ROUND = 2, CHARACTER_SHIFT = 3 };
// 日期取整级别
enum DateRoundLevel { DATA_UNKNOWN = 0, YEAR = 1, MONTH = 2, DAY = 3, HOUR = 4, MINUTE = 5 };
struct DateRound {
  // 日期取整级别
  DateRoundLevel level;
};
enum ShiftDirection { SHIFT_UNKNOWN = 0, LEFT = 1, RIGHT = 2 };
struct CharacterShift {
  // 移动方向
  ShiftDirection direction;
  // 移动位数
  uint32_t shift_amount;
};

struct NumericRound {
  // 保留小数点前几位
  uint32_t decimal_places = 1;
};

struct Transform {
  TransformType type;
  // 数字取整规则
  NumericRound numeric_round;
  // 日期取整规则
  DateRound date_round;
  // 字符位移规则
  CharacterShift character_shift;
};

class NumericRoundRewrite {
public:
  NumericRoundRewrite(const NumericRound& numeric);

public:
  std::string processNumeric(const std::string& data) const;

private:
  const uint32_t decimal_places_;
};

class DateRoundRewrite {
public:
  DateRoundRewrite(const DateRound& date);

public:
  std::string processDate(const std::string& data) const;

private:
  const uint32_t level_;
};

class CharacterShiftRewrite {
public:
  CharacterShiftRewrite(const CharacterShift& character);

public:
  std::string processCharacter(const std::string& data);

private:
  const ShiftDirection direction_;
  uint32_t shift_amount_;
};

class TransformRewrite {
public:
  TransformRewrite(const Transform& transform);

public:
  std::string transForm(const std::string& data) const;

private:
  const uint32_t transform_type_;
  std::unique_ptr<NumericRoundRewrite> numeric_round_;
  std::unique_ptr<DateRoundRewrite> date_round_;
  std::unique_ptr<CharacterShiftRewrite> character_shift_;
};
} // namespace Replaces
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy