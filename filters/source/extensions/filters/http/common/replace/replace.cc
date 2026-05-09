#include "replace.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
ReplaceRewrite::ReplaceRewrite(const Replace& replace)
    : replace_mode_(replace.rule_type), replace_value_(replace.replace_value),
      value_type_(replace.value_type) {
  switch (replace_mode_) {
  case RuleType::REPLACE_ALL:
    replacePtr_ = std::make_unique<FixedReplaceRewrite>(replace.replace);
    break;
  case RuleType::REPLACE_FIRST3:
    replacePtr_ = std::make_unique<FixedReplaceRewrite>(replace.replace);
    break;
  case RuleType::REPLACE_LAST4:
    replacePtr_ = std::make_unique<FixedReplaceRewrite>(replace.replace);
    break;
  case RuleType::REGEX_MATCH:
    regexPtr_ = std::make_unique<RegexMatchRewrite>(replace.regex_match);
    break;
  case RuleType::REPLACE_CUSTOM:
    customPtr_ = std::make_unique<CustomRuleRewrite>(replace.custom);
    break;
  default:
    break;
  }
}

std::string ReplaceRewrite::execudata(const std::string& data) const {
  std::string result;
  switch (replace_mode_) {
  case RuleType::REPLACE_ALL:
    result = replacePtr_->replaceAll(data, replace_value_, value_type_);
    break;
  case RuleType::REPLACE_FIRST3:
    result = replacePtr_->replaceFirst3(data, replace_value_, value_type_);
    break;
  case RuleType::REPLACE_LAST4:
    result = replacePtr_->replaceLast4(data, replace_value_, value_type_);
    break;
  case RuleType::REGEX_MATCH:
    result = regexPtr_->regexMatch(data, replace_value_, value_type_);
    break;
  case RuleType::REPLACE_CUSTOM:
    result = customPtr_->customData(data, replace_value_, value_type_);
    break;
  default:
    break;
  }
  return result;
}

FixedReplaceRewrite::FixedReplaceRewrite(const FixedReplace& replace) { (void)replace; }

std::vector<std::string> splitString(const std::string& str, char delimiter) {
  std::vector<std::string> tokens;
  std::stringstream ss(str);
  std::string token;
  while (std::getline(ss, token, delimiter)) {
    if (!token.empty()) {
      tokens.push_back(token);
    }
  }
  return tokens;
}

std::string randomValue(const std::string& data, const std::string& replace_value) {
  if (replace_value.empty())
    return data;

  std::string result;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, replace_value.size() - 1);

  for (size_t i = 0; i < data.size(); ++i) {
    result += replace_value[dis(gen)];
  }
  return result;
}

std::string cunstomRandomValue(const uint32_t& data, const std::string& replace_value) {
  if (replace_value.empty())
    return "";

  std::string result;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, replace_value.size() - 1);

  for (size_t i = 0; i < data; ++i) {
    result += replace_value[dis(gen)];
  }
  return result;
}

std::string sampleValue(const std::string& data, const std::string& replace_value) {
  auto dataSegments = splitString(data, ',');
  auto replaceValues = splitString(replace_value, ',');

  if (replaceValues.empty() || dataSegments.empty()) {
    return data;
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, replaceValues.size() - 1);

  std::string result;
  for (size_t i = 0; i < dataSegments.size(); ++i) {
    if (i > 0) {
      result += ",";
    }

    result += replaceValues[dis(gen)];
  }

  return result;
}

std::string fixedValue(const std::string& replace_value, size_t pos) {
  auto replaceValues = splitString(replace_value, ',');
  return replaceValues[pos % replaceValues.size()];
}

bool compareReplacePositions(const replacePostion& lhs, const replacePostion& rhs) {
  if (lhs.start == rhs.start) {
    return lhs.end < rhs.end;
  }
  return lhs.start < rhs.start;
}

void sortReservedPositions(std::vector<replacePostion>& reserved_positions) {
  std::sort(reserved_positions.begin(), reserved_positions.end(), compareReplacePositions);
}
static std::vector<replacePostion>
findNonReservedPositions(const uint32_t& len, std::vector<replacePostion>& reserved_positions) {
  std::vector<replacePostion> result;
  if (!reserved_positions.empty() && reserved_positions.front().start > 1) {
    result.push_back({1, reserved_positions.front().start - 1});
  }

  for (size_t i = 0; i < reserved_positions.size() - 1; ++i) {
    if (reserved_positions[i].end < reserved_positions[i + 1].start - 1) {
      result.push_back({reserved_positions[i].end + 1, reserved_positions[i + 1].start - 1});
    }
  }

  if (!reserved_positions.empty() && reserved_positions.back().end < len) {
    result.push_back({reserved_positions.back().end + 1, static_cast<uint32_t>(len)});
  }

  return result;
}

std::string FixedReplaceRewrite::replaceAll(const std::string& data,
                                            const std::string& replace_value,
                                            const ValueType value_type) const {
  size_t len = getWideCharCount(data);
  if (value_type == ValueType::FIXED_VALUE) {
    return replace_value;
  } else if (value_type == ValueType::RANDOM_VALUE) {
    return cunstomRandomValue(len, replace_value);
  } else {
    return sampleValue(data, replace_value);
  }
}

std::string FixedReplaceRewrite::replaceFirst3(const std::string& data,
                                               const std::string& replace_value,
                                               const ValueType value_type) const {
  auto w_data = stringToWString(data);
  size_t len = getWideCharCount(data);
  if (value_type == ValueType::FIXED_VALUE) {
    if (len < 3) {
      return replace_value;
    }
    std::string result = replace_value + wstringToString(w_data.substr(3));
    return result;
  } else if (value_type == ValueType::RANDOM_VALUE) {
    if (len < 3) {
      return cunstomRandomValue(len, replace_value);
    }
    std::string result = cunstomRandomValue(3, replace_value) +
                         wstringToString(w_data.substr(3));
    return result;
  }
  if (len < 3) {
    return sampleValue(data, replace_value);
  }
  std::string result = sampleValue(wstringToString(w_data.substr(0, 3)), replace_value) +
                       wstringToString(w_data.substr(3));
  return result;
}

std::string FixedReplaceRewrite::replaceLast4(const std::string& data,
                                              const std::string& replace_value,
                                              const ValueType value_type) const {
  auto w_data = stringToWString(data);
  size_t len = getWideCharCount(data);
  if (value_type == ValueType::FIXED_VALUE) {
    if (len < 4) {
      return replace_value;
    }
    std::string result = wstringToString(w_data.substr(0, len - 4)) + replace_value;
    return result;
  } else if (value_type == ValueType::RANDOM_VALUE) {
    if (len < 4) {
      return replace_value;
    }
    std::string result = wstringToString(w_data.substr(0, len - 4)) +
                         cunstomRandomValue(4, replace_value);
    return result;
  } else {
    if (len < 4) {
      return replace_value;
    }
    std::string result = wstringToString(w_data.substr(0, len - 4)) +
                         sampleValue(wstringToString(w_data.substr(len - 4, len)), replace_value);
    return result;
  }
  return data;
}

RegexMatchRewrite::RegexMatchRewrite(const RegexMatch& regex) : pattern_(regex.regex) {}

std::string RegexMatchRewrite::regexMatch(const std::string& data, const std::string& replace_value,
                                          const ValueType value_type) const {
  try {
    std::regex regex(pattern_);
    std::string result;
    std::ostringstream replacedStream;

    auto begin = std::sregex_iterator(data.begin(), data.end(), regex);
    auto end = std::sregex_iterator();

    size_t lastMatchEnd = 0;
    size_t pos = 0;
    for (std::sregex_iterator i = begin; i != end; ++i) {
      std::smatch match = *i;
      replacedStream << data.substr(lastMatchEnd, match.position() - lastMatchEnd);
      switch (value_type) {
      case ValueType::FIXED_VALUE:
        replacedStream << fixedValue(replace_value, pos);
        pos++;
        break;
      case ValueType::RANDOM_VALUE:
        replacedStream << randomValue(match.str(), replace_value);
        break;
      case ValueType::SAMPLE_VALUE:
        replacedStream << sampleValue(match.str(), replace_value);
        break;
      default:
        replacedStream << match.str();
        break;
      }
      lastMatchEnd = match.position() + match.length();
    }
    replacedStream << data.substr(lastMatchEnd);
    return replacedStream.str();
  } catch (const std::regex_error& e) {
    return data;
  }
}

ReplaceRuleDefinitionRewrite::ReplaceRuleDefinitionRewrite(const ReplaceRuleDefinition& rule)
    : start_(rule.start), end_(rule.end) {}

std::string
ReplaceRuleDefinitionRewrite::reserveRule(const std::string& data,
                                          std::vector<replacePostion>& reserved_postion) const {

  uint32_t actualStart = start_;
  size_t len = getWideCharCount(data);
  uint32_t actualEnd = std::min(static_cast<uint32_t>(len), end_);

  replacePostion replace_pos;
  replace_pos.start = actualStart;
  replace_pos.end = actualEnd;
  reserved_postion.push_back(replace_pos);
  return "";
}

std::string
ReplaceRuleDefinitionRewrite::coverRule(const std::string& data,
                                        std::vector<replacePostion>& reserved_postion) const {

  uint32_t actualStart = start_ - 1;
  size_t len = getWideCharCount(data);
  uint32_t actualEnd = std::min(static_cast<uint32_t>(len), end_);

  replacePostion replace_pos;
  replace_pos.start = actualStart;
  replace_pos.end = actualEnd;
  reserved_postion.push_back(replace_pos);
  return "";
}

CustomRuleRewrite::CustomRuleRewrite(const CustomRule& custom)
    : rulesPtr_([&custom]() {
        std::vector<std::unique_ptr<ReplaceRuleDefinitionRewrite>> rules;
        for (const auto& rule : custom.rules) {
          rules.push_back(std::make_unique<ReplaceRuleDefinitionRewrite>(rule));
        }
        return rules;
      }()),
      cover_type_(custom.cover_type) {}

std::string CustomRuleRewrite::customData(const std::string& data, const std::string& value,
                                          const ValueType& value_type) const {
  size_t pos = 0;
  auto result = stringToWString(data);
  size_t len = getWideCharCount(data);
  if (cover_type_ == ReplaceCoverType::REPLACE_COVER) {
    std::vector<replacePostion> reserved_postion;

    for (auto& rule : rulesPtr_) {
      rule->coverRule(data, reserved_postion);
    }
    // 排序
    sortReservedPositions(reserved_postion);
    std::ostringstream replacedStream;
    uint32_t lastEnd = 0;
    for (auto& res_postion : reserved_postion) {
      replacedStream << wstringToString(result.substr(lastEnd, res_postion.start - lastEnd));
      switch (value_type) {
      case ValueType::FIXED_VALUE:
        replacedStream << fixedValue(value, pos);
        pos++;
        break;
      case ValueType::RANDOM_VALUE:
        replacedStream << cunstomRandomValue((res_postion.end - res_postion.start), value);
        break;
      case ValueType::SAMPLE_VALUE:
        replacedStream << sampleValue(
            wstringToString(result.substr(res_postion.start, res_postion.end)), value);
        break;
      default:
        break;
      }
      lastEnd = res_postion.end;
    }
    replacedStream << wstringToString(result.substr(lastEnd));
    return replacedStream.str();
  }
  if (cover_type_ == ReplaceCoverType::REPLACE_RESERVE) {
    std::vector<replacePostion> reserved_postion;
    for (auto& rule : rulesPtr_) {
      rule->reserveRule(data, reserved_postion);
    }
    // 排序
    sortReservedPositions(reserved_postion);
    // 取反
    reserved_postion = findNonReservedPositions(len, reserved_postion);
    std::ostringstream replacedStream;
    uint32_t lastEnd = 0;
    for (auto& res_postion : reserved_postion) {
      replacedStream << wstringToString(result.substr(lastEnd, res_postion.start - lastEnd - 1));
      switch (value_type) {
      case ValueType::FIXED_VALUE:
        replacedStream << fixedValue(value, pos);
        pos++;
        break;
      case ValueType::RANDOM_VALUE:
        replacedStream << cunstomRandomValue((res_postion.end - (res_postion.start - 1)), value);
        break;
      case ValueType::SAMPLE_VALUE:
        replacedStream << sampleValue(
            wstringToString(result.substr(res_postion.start - 1, res_postion.end)), value);
        break;
      default:
        break;
      }
      lastEnd = res_postion.end;
    }
    replacedStream << wstringToString(result.substr(lastEnd));
    return replacedStream.str();
  }

  return data;
}

} // namespace Replaces
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy