#pragma once

#include <string>

#include "envoy/srhino_plugin_framework/v1_4_x/context.h"

#include "envoy/srhino_plugin_framework/v1_4_x/proto/config/core/cidr.pb.h"
#include "envoy/srhino_plugin_framework/v1_4_x/proto/config/type/matcher/matcher.pb.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
class Rule {
public:
  Rule(const std::string& /*upstream_name*/,
       const srhino_plugin_framework::v1_4_x::proto::config::core::CidrRange& /*cidr*/,
       bool /*ip_invert*/,
       const google::protobuf::RepeatedPtrField<
           srhino_plugin_framework::v1_4_x::proto::config::type::matcher::Matcher>& /*matchers*/) {}
  Rule(const std::string& /*upstream_name*/,
       const srhino_plugin_framework::v1_4_x::proto::config::type::matcher::IpSet& /*ip_set*/,
       bool /*ip_invert*/,
       const google::protobuf::RepeatedPtrField<
           srhino_plugin_framework::v1_4_x::proto::config::type::matcher::Matcher>& /*matchers*/) {}
  Rule(const std::string& /*upstream_name*/,
       const google::protobuf::RepeatedPtrField<
           srhino_plugin_framework::v1_4_x::proto::config::type::matcher::Matcher>& /*matchers*/) {}
  Rule(const Rule&) = delete;
  virtual ~Rule() = default;

public:
  /**
   * 判断本规则是否匹配
   * @param upstream 上游集群名
   * @param address 下游地址
   * @param context HTTP环境上下文
   * @param pass_empty 所有匹配条件为空时，视为匹配
   * @return true
   * @return false
   */
  virtual bool match(const std::string& upstream_name, uint32_t address,
                     const HeaderContext& context, bool pass_empty) const = 0;

};
} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework