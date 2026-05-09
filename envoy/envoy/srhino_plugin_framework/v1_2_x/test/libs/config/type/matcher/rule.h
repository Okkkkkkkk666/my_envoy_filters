#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/libs/config/type/matcher/rule.h"

namespace SrhinoPluginFramework {
namespace Test {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
class MockRule : public SrhinoPluginFramework::Libs::Config::Type::Matcher::Rule {
public:
  MockRule()
      : SrhinoPluginFramework::Libs::Config::Type::Matcher::Rule(
            "", google::protobuf::RepeatedPtrField<
                    srhino_plugin_framework::v1_2_x::proto::config::type::matcher::Matcher>()) {}
  MOCK_METHOD(bool, match, (const std::string&, std::uint32_t, const HeaderContext&, bool),
              (const));
  MOCK_METHOD(std::uint64_t, hash, (), (const));
};
} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace Test
} // namespace SrhinoPluginFramework