#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_3_x/libs/config/config.h"

namespace SrhinoPluginFramework {
namespace Test {
namespace Libs {
namespace Config {

using namespace SrhinoPluginFramework::Libs::Config;
class MockConfig : public SrhinoPluginFramework::Libs::Config::Config {
public:
  MOCK_METHOD(std::unique_ptr<SrhinoPluginFramework::Libs::Config::Type::Matcher::Matcher>,
              createMatcher,
              (const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::Matcher&),
              (const));
  MOCK_METHOD(std::unique_ptr<SrhinoPluginFramework::Libs::Config::Type::Matcher::Rule>, createRule,
              (const std::string&,
               const srhino_plugin_framework::v1_3_x::proto::config::core::CidrRange&, bool,
               const google::protobuf::RepeatedPtrField<
                   srhino_plugin_framework::v1_3_x::proto::config::type::matcher::Matcher>&),
              (const));
  MOCK_METHOD(std::unique_ptr<SrhinoPluginFramework::Libs::Config::Type::Matcher::Rule>, createRule,
              (const std::string&,
               const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::IpSet&,
               bool ip_invert,
               const google::protobuf::RepeatedPtrField<
                   srhino_plugin_framework::v1_3_x::proto::config::type::matcher::Matcher>&),
              (const));
  MOCK_METHOD(std::unique_ptr<SrhinoPluginFramework::Libs::Config::Type::Matcher::Rule>, createRule,
              (const std::string&,
               const google::protobuf::RepeatedPtrField<
                   srhino_plugin_framework::v1_3_x::proto::config::type::matcher::Matcher>&),
              (const));
  MOCK_METHOD(std::unique_ptr<SrhinoPluginFramework::Libs::Config::Type::Matcher::IpSet>,
              createIpSet,
              (const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::IpSet&),
              (const));

  MOCK_METHOD(std::unique_ptr<SrhinoPluginFramework::Libs::Config::Type::Matcher::UserMatcher>,
              createUserMatcher,
              (const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::UserMatcher&),
              (const, override));

  MOCK_METHOD(std::unique_ptr<SrhinoPluginFramework::Libs::Config::Type::Matcher::RegionMatcher>,
              createRegionMatcher,
              (const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::RegionMatcher&),
              (const, override));
  MOCK_METHOD(std::unique_ptr<SrhinoPluginFramework::Libs::Config::Type::Matcher::DateMatcher>,
              createDateMatcher,
              (const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::DateMatcher&),
              (const, override));
};

} // namespace Config
} // namespace Libs
} // namespace Test
} // namespace SrhinoPluginFramework