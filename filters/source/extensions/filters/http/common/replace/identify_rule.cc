#include "identify_rule.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
IdentifyRule::IdentifyRule(const identify_rule& rule)
    : type_(rule.identify_type), logic_(rule.recognition_logic), field_name_(rule.field_name),
      data_content_(rule.data_content) {}

static std::string replaceCommasWithPipes(const std::string& input) {
  std::string result = input;
  size_t pos = 0;
  while ((pos = result.find(',', pos)) != std::string::npos) {
    result[pos] = '|';
    pos++;
  }
  return result;
}

static std::string replaceKey(const std::string& key) {
  std::string prefix = R"R(("(KEYWORD)"[\s:]+"([^\n"]+)["])|("(KEYWORD)"[\s:]+([^\n"](-?\d+)(\.\d+)?)))R";
  std::string::size_type pos = 0;
  while ((pos = prefix.find("KEYWORD", pos)) != std::string::npos) {
    prefix.replace(pos, 7, key);
    pos += key.size();
  }
  return prefix;
}

static std::string replaceKeyValue(const std::string& key, const std::string& value) {
  std::string prefix = R"R(("(KEYWORD)"[\s:]+"(VALUE)")|("(KEYWORD)"[\s:]+(VALUE)))R";

  std::string::size_type key_pos = 0;
  while ((key_pos = prefix.find("KEYWORD", key_pos)) != std::string::npos) {
    prefix.replace(key_pos, 7, key);
    key_pos += key.size();
  }

  std::string::size_type value_pos = 0;
  while ((value_pos = prefix.find("VALUE", value_pos)) != std::string::npos) {
    prefix.replace(value_pos, 5, value);
    value_pos += value.size();
  }

  return prefix;
}
static std::string extractValue(const std::string& data) {
  std::regex valueRegex(R"R(":\s*"([^"]*)"|:\s*([^,}\]]*))R");
  std::smatch match;
  if (std::regex_search(data, match, valueRegex)) {
    if (match.size() > 1 && !match.str(1).empty()) {
      return match.str(1);
    } else if (match.size() > 2) {
      return match.str(2);
    }
  }
  return data;
}

std::vector<std::string> IdentifyRule::matchALL(const std::string& data) {
  std::string rules;
  if (type_ == IdentifyType::DATA_CONTENT) {
    rules = data_content_;
  } else if (type_ == IdentifyType::FIELD_NAME) {
    auto result = replaceCommasWithPipes(field_name_);
    rules = replaceKey(result);
  } else if (type_ == IdentifyType::DATA_CONTENT_AND_FIELD_NAME) {
    if (logic_ == RecognitionLogic::SATISFY_ALL) {
      auto result = replaceCommasWithPipes(field_name_);
      rules = replaceKeyValue(result, data_content_);
    } else if (logic_ == RecognitionLogic::SATISFY_ANY) {
      auto result = replaceCommasWithPipes(field_name_);
      rules = replaceKey(result);
      rules = rules + "|" + data_content_;
    }
  }
  matcher_ = std::make_shared<RegexMatcher::RegexMatcher>(RegexMatcher::RegexType::RegexHyperscan,
                                                          true, false);
  std::vector<RegexMatcher::ExpressionView> exprs;
  exprs.emplace_back(rules, 1, true);
  std::vector<RegexMatcher::ExpressionView> failed_exprs;
  int ret = matcher_->init(exprs, failed_exprs);
  if (ret < 0) {
    matcher_ = nullptr;
  }
  std::vector<std::string> new_data;
  std::vector<RegexMatcher::MatchResult> results;
  const char* pdata = data.data();
  if (matcher_ != nullptr) {
    matcher_->match(std::string_view(data), results, RegexMatcher::EncodingType::UTF8, 0);
    // 排序
    std::sort(results.begin(), results.end(), RegexMatcher::MatchResult::cmp);
    //  解决冲突
    int sz = results.size();
    int j = 0;
    for (int i = 1; i < sz; ++i) {
      if (results[i].start() < results[j].end()) {
        if (results[i].id() < results[j].id()) {
          results[j].set_flag(false);
          j = i;
        } else {
          results[i].set_flag(false);
        }
      } else {
        j = i;
      }
    }

    for (const auto& item : results) {
      if (item.flag()) {
        new_data.emplace_back(
            extractValue(std::string(pdata + item.start(), item.end() - item.start())));
      }
    }
  }
  return new_data;
}

} // namespace Replaces
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy