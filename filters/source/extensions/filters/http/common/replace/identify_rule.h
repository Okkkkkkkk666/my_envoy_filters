#pragma once
#include <iostream>
#include <algorithm>
#include <vector>
#include <regex>
#include "filters/source/extensions/filters/http/common/regex_matcher/regex_matcher.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
namespace RegexMatcher = Envoy::Extensions::Filters::Common::RegexMatcher;
using RegexMatcherPtr = std::shared_ptr<RegexMatcher::RegexMatcher>;

enum IdentifyType { DATA_CONTENT = 1, FIELD_NAME = 2, DATA_CONTENT_AND_FIELD_NAME = 3 };
enum RecognitionLogic { SATISFY_ALL = 1, SATISFY_ANY = 2 };
struct identify_rule {
  IdentifyType identify_type;
  RecognitionLogic recognition_logic;
  std::string field_name;
  std::string data_content;
};

class IdentifyRule {
public:
  IdentifyRule(const identify_rule& rule);
  std::vector<std::string> matchALL(const std::string& data);

private:
  const IdentifyType type_;
  const RecognitionLogic logic_;
  const std::string field_name_;
  const std::string data_content_;
  RegexMatcherPtr matcher_;
};
} // namespace Replaces
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy