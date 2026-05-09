#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_1_x/libs/regex/regex.h"
#include "envoy/srhino_plugin_framework/v1_1_x/test/libs/regex/regex_matcher.h"
#include "envoy/srhino_plugin_framework/v1_1_x/test/libs/regex/regex_replacer.h"

namespace SrhinoPluginFramework {
namespace Test {
namespace Libs {
namespace Regex {

using namespace SrhinoPluginFramework::Libs::Regex;
using testing::_;
using testing::Return;

class MockRegex : public SrhinoPluginFramework::Libs::Regex::Regex {
public:
  MockRegex() {
    EXPECT_CALL(*this, createRegexMatcher(_, _, _))
        .WillRepeatedly([](RegexMode, bool, const std::string&) {
          return std::make_shared<MockRegexMatcher>();
        });
    EXPECT_CALL(*this, createRegexReplacer(_, _, _, _))
        .WillRepeatedly([](bool, const std::string&, const std::string&, EncodingType) {
          return std::make_shared<MockRegexReplacer>();
        });
  }

public:
  MOCK_METHOD(RegexMatcherSharedPtr, createRegexMatcher, (RegexMode, bool, const std::string&), ());
  MOCK_METHOD(RegexReplacerSharedPtr, createRegexReplacer,
              (bool, const std::string&, const std::string&, EncodingType), ());
};

} // namespace Regex
} // namespace Libs
} // namespace Test
} // namespace SrhinoPluginFramework