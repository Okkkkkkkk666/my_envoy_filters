#pragma once

#include "envoy/srhino_plugin_framework/v1_1_x/libs/config/config.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Config {
class ConfigImpl : public Config {
public:
  ConfigImpl() {}

public:
  std::unique_ptr<Type::Matcher::Matcher> createMatcher(
      const srhino_plugin_framework::v1_1_x::proto::config::type::matcher::Matcher& matcher)
      const override;

  std::unique_ptr<Type::Matcher::Rule>
  createRule(const std::string& upstream_name,
             const srhino_plugin_framework::v1_1_x::proto::config::core::CidrRange& cidr,
             bool ip_invert,
             const google::protobuf::RepeatedPtrField<
                 srhino_plugin_framework::v1_1_x::proto::config::type::matcher::Matcher>& matchers)
      const override;

  std::unique_ptr<Type::Matcher::Rule>
  createRule(const std::string& upstream_name,
             const srhino_plugin_framework::v1_1_x::proto::config::type::matcher::IpSet& ip_set,
             bool ip_invert,
             const google::protobuf::RepeatedPtrField<
                 srhino_plugin_framework::v1_1_x::proto::config::type::matcher::Matcher>& matchers)
      const override;

  std::unique_ptr<Type::Matcher::Rule>
  createRule(const std::string& upstream_name,
             const google::protobuf::RepeatedPtrField<
                 srhino_plugin_framework::v1_1_x::proto::config::type::matcher::Matcher>& matchers)
      const override;

  std::unique_ptr<Type::Matcher::IpSet>
  createIpSet(const srhino_plugin_framework::v1_1_x::proto::config::type::matcher::IpSet& ip_set)
      const override;

  std::unique_ptr<Type::Matcher::UserMatcher> createUserMatcher(
      const srhino_plugin_framework::v1_1_x::proto::config::type::matcher::UserMatcher& user)
      const override;
  std::unique_ptr<Type::Matcher::RegionMatcher> createRegionMatcher(
      const srhino_plugin_framework::v1_1_x::proto::config::type::matcher::RegionMatcher& region)
      const override;
};

} // namespace Config
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework