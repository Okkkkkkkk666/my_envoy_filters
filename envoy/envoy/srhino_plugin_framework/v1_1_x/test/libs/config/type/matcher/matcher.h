#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/libs/config/type/matcher/matcher.h"

namespace SrhinoPluginFramework {
namespace Test {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
class MockMatcher : public SrhinoPluginFramework::Libs::Config::Type::Matcher::Matcher {
public:
  MockMatcher()
      : SrhinoPluginFramework::Libs::Config::Type::Matcher::Matcher(
            srhino_plugin_framework::v1_1_x::proto::config::type::matcher::Matcher()) {}

  MOCK_METHOD(const srhino_plugin_framework::v1_1_x::proto::config::type::matcher::Matcher&, config,
              (), (const));
  MOCK_METHOD(bool, matchPath, (const HeaderContext&), (const));
  MOCK_METHOD(bool, matchHeader, (const HeaderContext&), (const));
  MOCK_METHOD(bool, matchQueryParameter, (const HeaderContext&), (const));
  MOCK_METHOD(bool, matchMethods, (const HeaderContext&), (const));
  MOCK_METHOD(bool, matchUser, (const std::string&), (const));
  MOCK_METHOD(bool, matchIp, (uint32_t), (const));
  MOCK_METHOD(bool, matchIpRegion, (const std::string&), (const));  
  MOCK_METHOD(bool, matchReferer, (const HeaderContext&), (const)); 
  MOCK_METHOD(bool, matchAll, (const HeaderContext&), (const));
  MOCK_METHOD(bool, matchAll, (const HeaderContext&, uint32_t, const std::string&), (const));
  MOCK_METHOD(bool, matchAll, (const HeaderContext&, uint32_t, const std::string&, const std::string&), (const));
  MOCK_METHOD(bool, getUserMatchers, (), (const));
  MOCK_METHOD(uint64_t, hash, (), (const));
};
} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace Test
} // namespace SrhinoPluginFramework