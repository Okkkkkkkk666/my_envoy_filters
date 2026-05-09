#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_1_x/libs/regex/regex_replacer.h"

namespace SrhinoPluginFramework {
namespace Test {
namespace Libs {
namespace Regex {

using namespace SrhinoPluginFramework::Libs::Regex;

class MockRegexReplacer : public SrhinoPluginFramework::Libs::Regex::RegexReplacer {
public:
  MOCK_METHOD(bool, replace, (std::string&, EncodingType), ());
  MOCK_METHOD(bool, ignore_case, (), (const));
  MOCK_METHOD(const std::string&, orig_match_expr, (), (const));
  MOCK_METHOD(const std::string&, replace_expr, (), (const));
};

} // namespace Regex
} // namespace Libs
} // namespace Test
} // namespace SrhinoPluginFramework