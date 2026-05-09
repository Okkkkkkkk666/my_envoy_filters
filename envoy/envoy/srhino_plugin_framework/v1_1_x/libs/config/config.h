#pragma once

#include <memory>
#include "type/matcher/matcher.h"
#include "type/matcher/region_matcher.h"
#include "type/matcher/rule.h"
#include "type/matcher/ip_list.h"
#include "type/matcher/user_matcher.h"
namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Config {

class Config {
public:
  Config() {}
  Config(const Config&) = delete;
  virtual ~Config() = default;

public:
  virtual std::unique_ptr<Type::Matcher::Matcher> createMatcher(
      const srhino_plugin_framework::v1_1_x::proto::config::type::matcher::Matcher& matcher)
      const = 0;

  virtual std::unique_ptr<Type::Matcher::Rule>
  createRule(const std::string& upstream_name,
             const srhino_plugin_framework::v1_1_x::proto::config::core::CidrRange& cidr,
             bool ip_invert,
             const google::protobuf::RepeatedPtrField<
                 srhino_plugin_framework::v1_1_x::proto::config::type::matcher::Matcher>& matchers)
      const = 0;

  virtual std::unique_ptr<Type::Matcher::Rule>
  createRule(const std::string& upstream_name,
             const srhino_plugin_framework::v1_1_x::proto::config::type::matcher::IpSet& ip_set,
             bool ip_invert,
             const google::protobuf::RepeatedPtrField<
                 srhino_plugin_framework::v1_1_x::proto::config::type::matcher::Matcher>& matchers)
      const = 0;

  virtual std::unique_ptr<Type::Matcher::Rule>
  createRule(const std::string& upstream_name,
             const google::protobuf::RepeatedPtrField<
                 srhino_plugin_framework::v1_1_x::proto::config::type::matcher::Matcher>& matchers)
      const = 0;

  virtual std::unique_ptr<Type::Matcher::IpSet> createIpSet(
      const srhino_plugin_framework::v1_1_x::proto::config::type::matcher::IpSet& ip_set) const = 0;

  virtual std::unique_ptr<Type::Matcher::UserMatcher> createUserMatcher(
      const srhino_plugin_framework::v1_1_x::proto::config::type::matcher::UserMatcher& user)
      const = 0;
  virtual std::unique_ptr<Type::Matcher::RegionMatcher> createRegionMatcher(
      const srhino_plugin_framework::v1_1_x::proto::config::type::matcher::RegionMatcher& region)
      const = 0;
};
using ConfigSharedPtr = std::shared_ptr<Config>;

} // namespace Config
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework