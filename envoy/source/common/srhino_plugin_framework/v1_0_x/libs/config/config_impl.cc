#include "source/common/srhino_plugin_framework/v1_0_x/libs/config/config_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/config/type/matcher/matcher_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/config/type/matcher/rule_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/config/type/matcher/ip_list_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/config/type/matcher/user_matcher_impl.h"
namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace Config {

std::unique_ptr<Type::Matcher::Matcher> ConfigImpl::createMatcher(
    const srhino_plugin_framework::v1_0_x::proto::config::type::matcher::Matcher& matcher) const {
  return std::make_unique<Type::Matcher::MatcherImpl>(matcher);
}

std::unique_ptr<Type::Matcher::Rule> ConfigImpl::createRule(
    const std::string& upstream_name,
    const srhino_plugin_framework::v1_0_x::proto::config::core::CidrRange& cidr, bool ip_invert,
    const google::protobuf::RepeatedPtrField<
        srhino_plugin_framework::v1_0_x::proto::config::type::matcher::Matcher>& matchers) const {
  return std::make_unique<Type::Matcher::RuleImpl>(upstream_name, cidr, ip_invert, matchers);
}

std::unique_ptr<Type::Matcher::Rule> ConfigImpl::createRule(
    const std::string& upstream_name,
    const srhino_plugin_framework::v1_0_x::proto::config::type::matcher::IpSet& ip_set,
    bool ip_invert,
    const google::protobuf::RepeatedPtrField<
        srhino_plugin_framework::v1_0_x::proto::config::type::matcher::Matcher>& matchers) const {
  return std::make_unique<Type::Matcher::RuleImpl>(upstream_name, ip_set, ip_invert, matchers);
}

std::unique_ptr<Type::Matcher::Rule> ConfigImpl::createRule(
    const std::string& upstream_name,
    const google::protobuf::RepeatedPtrField<
        srhino_plugin_framework::v1_0_x::proto::config::type::matcher::Matcher>& matchers) const {
  return std::make_unique<Type::Matcher::RuleImpl>(upstream_name, matchers);
}

std::unique_ptr<Type::Matcher::IpSet> ConfigImpl::createIpSet(
    const srhino_plugin_framework::v1_0_x::proto::config::type::matcher::IpSet& ip_set) const {
  return std::make_unique<Type::Matcher::IpSetImpl>(ip_set);
}

std::unique_ptr<Type::Matcher::UserMatcher> ConfigImpl::createUserMatcher(
    const srhino_plugin_framework::v1_0_x::proto::config::type::matcher::UserMatcher& user) const {
  return std::make_unique<Type::Matcher::UserMatcherImpl>(user);
}
} // namespace Config
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework