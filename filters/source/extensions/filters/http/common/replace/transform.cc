#include "transform.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
TransformRewrite::TransformRewrite(const Transform& transform) : transform_type_(transform.type) {
  switch (transform_type_) {
  case TransformType::NUMERIC_ROUND:
    numeric_round_ = std::make_unique<NumericRoundRewrite>(transform.numeric_round);
    break;
  case TransformType::DATE_ROUND:
    date_round_ = std::make_unique<DateRoundRewrite>(transform.date_round);
    break;
  case TransformType::CHARACTER_SHIFT:
    character_shift_ = std::make_unique<CharacterShiftRewrite>(transform.character_shift);
    break;
  default:
    break;
  }
}

std::string TransformRewrite::transForm(const std::string& data) const {
  std::string result;
  switch (transform_type_) {
  case TransformType::NUMERIC_ROUND:
    result = numeric_round_->processNumeric(data);
    break;
  case TransformType::DATE_ROUND:
    result = date_round_->processDate(data);
    break;
  case TransformType::CHARACTER_SHIFT:
    result = character_shift_->processCharacter(data);
    break;
  default:
    break;
  }
  return result;
}

NumericRoundRewrite::NumericRoundRewrite(const NumericRound& numeric)
    : decimal_places_(numeric.decimal_places) {}
std::string NumericRoundRewrite::processNumeric(const std::string& data) const {
  double doubleValue = std::stod(data);

  double divisor = std::pow(10.0, decimal_places_ - 1);

  double result;
  if (decimal_places_ == 0) {
    result = static_cast<int>(doubleValue);
  } else {
    result = std::floor(doubleValue / divisor) * divisor;
  }

  std::string stringValue = std::to_string(result);
  size_t dotPos = stringValue.find('.');
  if (dotPos < decimal_places_) {
    return data;
  }
  if (dotPos != std::string::npos) {
    stringValue = stringValue.substr(0, dotPos);
  }
  return stringValue;
}

DateRoundRewrite::DateRoundRewrite(const DateRound& date) : level_(date.level) {}

std::string DateRoundRewrite::processDate(const std::string& data) const {
  std::regex dateRegex("(\\d{4})([-/\\\\ ]?)(\\d{2})([-/\\\\ ]?)(\\d{2})[ "
                       "T]([01]?\\d|2[0-3]):([0-5]?\\d):([0-5]?\\d)");
  std::smatch match;
  std::tm time = {};

  if (std::regex_match(data, match, dateRegex)) {
    char separator = match[2].str()[0];

    time.tm_year = std::stoi(match[1]) - 1900;
    time.tm_mon = std::stoi(match[3]) - 1;
    time.tm_mday = std::stoi(match[5]);
    time.tm_hour = std::stoi(match[6]);
    time.tm_min = std::stoi(match[7]);
    time.tm_sec = std::stoi(match[8]);

    // 根据日期取整级别进行调整
    switch (level_) {
    case DateRoundLevel::YEAR:
      time.tm_mon = -1;
      [[fallthrough]];
    case DateRoundLevel::MONTH:
      time.tm_mday = 0;
      [[fallthrough]];
    case DateRoundLevel::DAY:
      time.tm_hour = 0;
      [[fallthrough]];
    case DateRoundLevel::HOUR:
      time.tm_min = 0;
      [[fallthrough]];
    case DateRoundLevel::MINUTE:
      time.tm_sec = 0;
      break;
    }

    // 根据连接符类型调整输出格式字符串
    std::string format = "";
    switch (separator) {
    case '-':
      format = "%Y-%m-%d %H:%M:%S";
      break;
    case '/':
      format = "%Y/%m/%d %H:%M:%S";
      break;
    case '\\':
      format = "%Y\\%m\\%d %H:%M:%S";
      break;
    case ' ':
      format = "%Y %m %d %H:%M:%S";
      break;
    default:
      format = "%Y%m%d %H:%M:%S";
      break;
    }

    // 格式化日期
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), format.c_str(), &time);
    return std::string(buffer);
  } else {
    return data;
  }
}

CharacterShiftRewrite::CharacterShiftRewrite(const CharacterShift& character)
    : direction_(character.direction), shift_amount_(character.shift_amount) {}

std::string CharacterShiftRewrite::processCharacter(const std::string& data) {
  std::wstring wData = stringToWString(data);
  std::string shiftedString;
  auto length = getWideCharCount(data);
  if (length >= shift_amount_) { // 当字符串长度小于设置位数时，不进行位移
    if (shift_amount_ > 0 && length > 0) {
      shift_amount_ = shift_amount_ % length; // 防止移动位数大于字符串长度
      if (direction_ == ShiftDirection::RIGHT) {
        shiftedString = wstringToString(wData.substr(length - shift_amount_)) +
                        wstringToString(wData.substr(0, length - shift_amount_));
      } else if (direction_ == ShiftDirection::LEFT) {
        shiftedString = wstringToString(wData.substr(shift_amount_)) +
                        wstringToString(wData.substr(0, shift_amount_));
      }
    }
  }
  return shiftedString;
}
} // namespace Replaces
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy