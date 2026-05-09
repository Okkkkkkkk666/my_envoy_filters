#include "cover.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
CoverRewrite::CoverRewrite(const Cover& cover)
    : cover_character_(cover.cover_character), cover_mode_(cover.cover_mode),
      cover_type_(cover.cover_type) {
  switch (cover_mode_) {
  case CoverMode::CUSTOM:
    for (const auto& rule : cover.rules) {
      rules_ptr_.push_back(std::make_unique<CoverRuleDefinition>(rule));
    }
    break;
  default:
    break;
  }
}

std::string CoverRewrite::coverData(const std::string& data) const{
  std::string result;
  switch (cover_mode_) {
  case CoverMode::CUSTOM:
    result = customRule(data, cover_character_, cover_type_);
    break;
  case CoverMode::COVER_ALL:
    result = coverAll(data, cover_character_);
    break;
  case CoverMode::RESERVE_FIRST1_LAST1:
    result = coverFirst1Last1(data, cover_character_);
    break;
  case CoverMode::RESERVE_FIRST3_LAST2:
    result = coverFirst3Last2(data, cover_character_);
    break;
  case CoverMode::RESERVE_FIRST3_LAST4:
    result = coverFirst3Last4(data, cover_character_);
    break;
  case CoverMode::RESERVE_LAST4:
    result = coverLast4(data, cover_character_);
    break;
  default:
    break;
  }
  return result;
}

std::string CoverRewrite::customRule(const std::string& data, const std::string& replace_value,
                                     const CoverType cover_type) const{
  if (cover_type == COVER) {
    auto result = stringToWString(data);
    size_t len = getWideCharCount(data);
    std::vector<bool> status(data.size(), false);
    for (auto& rule : rules_ptr_) {
      rule->reserveLocation(data, status);
    }
    for (size_t i = 0; i < len; i++) {
      if (status[i] == true) {
        result[i] = replace_value[0];
      }
    }
    return wstringToString(result);

  } else if (cover_type == RESERVE) {

    auto result = stringToWString(data);
    size_t len = getWideCharCount(data);
    std::vector<bool> status(data.size(), false);
    for (auto& rule : rules_ptr_) {
      rule->reserveLocation(data, status);
    }
    for (size_t i = 0; i < len; i++) {
      if (status[i] == false) {
        result[i] = replace_value[0];
      }
    }
    return wstringToString(result);
  }
  return data;
}

std::string CoverRewrite::coverAll(const std::string& data, const std::string& replace_value) const{
  size_t len = getWideCharCount(data);
  return wstringToString(std::wstring(len, replace_value[0]));
}

std::string CoverRewrite::coverFirst1Last1(const std::string& data,
                                           const std::string& replace_value) const{
  std::string convertedBack;
  auto result = stringToWString(data);
  size_t len = getWideCharCount(data);
  if (len <= 1) {
    convertedBack = wstringToString(std::wstring(len, replace_value[0]));
    return convertedBack;
  } else if (len == 2) {
    convertedBack = wstringToString(result.front() + std::wstring(1, replace_value[0]));
    return convertedBack;
  }
  convertedBack = wstringToString(result.front() + std::wstring(len - 2, replace_value[0]) +
                                  std::wstring(1, result[len - 1]));

  return convertedBack;
}

std::string CoverRewrite::coverFirst3Last2(const std::string& data,
                                           const std::string& replace_value) const{
  auto result = stringToWString(data);
  size_t len = getWideCharCount(data);
  if (len <= 3) {
    return wstringToString(std::wstring(len, replace_value[0]));
  } else if (len <= 5) {
    return wstringToString(result.substr(0, 3) + std::wstring(len - 3, replace_value[0]));
  }
  return wstringToString(result.substr(0, 3) + std::wstring(len - 5, replace_value[0]) +
                         result.substr(len - 2, 2));
}

std::string CoverRewrite::coverFirst3Last4(const std::string& data,
                                           const std::string& replace_value) const{
  auto result = stringToWString(data);
  size_t len = getWideCharCount(data);
  if (len <= 3) {
    return wstringToString(std::wstring(len, replace_value[0]));
  } else if (len <= 7) {
    return wstringToString(result.substr(0, 3) + std::wstring(len - 3, replace_value[0]));
  }
  return wstringToString(result.substr(0, 3) + std::wstring(len - 7, replace_value[0]) +
                         result.substr(len - 4, 4));
}

std::string CoverRewrite::coverLast4(const std::string& data, const std::string& replace_value) const{
  auto result = stringToWString(data);
  size_t len = getWideCharCount(data);
  if (len <= 4) {
    return wstringToString(std::wstring(len, replace_value[0]));
  }
  return wstringToString(std::wstring(len - 4, replace_value[0]) + result.substr(len - 4, 4));
}

CoverRuleDefinition::CoverRuleDefinition(const CoverRule& rule)
    : start_(rule.start), end_(rule.end) {}

std::string CoverRuleDefinition::reserveLocation(const std::string& data,
                                                 std::vector<bool>& status) const{
  size_t len = getWideCharCount(data);
  uint32_t actualStart = start_ - 1;
  uint32_t actualEnd = std::min(static_cast<uint32_t>(len), end_);

  for (uint32_t i = actualStart; i < actualEnd; ++i) {
    // 标记需要保留的位置
    status[i] = true;
  }
  return data;
}
} // namespace Replace
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy