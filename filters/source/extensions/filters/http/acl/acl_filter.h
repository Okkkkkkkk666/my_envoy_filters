#pragma once

#include <vector>
#include <iostream>
#include <tuple>
#include "source/extensions/filters/http/common/pass_through_filter_ex.h"
#include "source/common/router/config_impl.h"

#include "filters/source/extensions/filters/http/common/strong_matcher/matcher.h"
#include "filters/source/extensions/filters/http/common/strong_matcher/rule.h"
#include "filters/source/extensions/filters/http/common/ip_whitelist/ip_whitelist.h"
#include "filters/api/envoy/extensions/filters/http/acl/v3/acl.pb.h"
#include "filters/api/envoy/extensions/filters/http/acl/v3/acl_log.pb.h"
#include "filters/source/extensions/filters/http/common/strong_matcher/user_matcher.h"
#define FILTER_NAME "envoy.filters.http.acl.1.0"

const std::string SrhinoKeyUserName("user_name");
const std::string SrhinoDomainMetaData("srhino_metadata");
namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AclFilter {

namespace v3 = envoy::extensions::filters::http::acl::v3;

class Rule {
public:
  Rule(const v3::Rule& rule);

  // 检查当前请求是否与本规则匹配.
  bool match(const Network::Address::InstanceConstSharedPtr& address,
             const Http::RequestHeaderMap& headers, const std::string& username) const;

  // 返回该规则的匹配动作. ALLOW、DENY、RECODE
  v3::Action action() const { return action_; }
  std::uint32_t id() const { return id_; }
  bool enable() const { return enable_; }
  const std::string& deny_context() const { return deny_context_; }
  const std::string& rule_name() const { return rule_name_; }
  const std::string& type() const { return type_; }
  const std::vector<Filters::Common::StrongMatcher::Matcher>& matcher() const { return matchers_; }

private:
  const std::vector<Filters::Common::StrongMatcher::Matcher> matchers_;
  const v3::Action action_;
  const std::uint32_t id_;
  const bool enable_;
  const std::string deny_context_;
  const std::string rule_name_;
  const std::string type_;
};

// 全局配置
class AclFilterGlobalConfig {
public:
  AclFilterGlobalConfig(const envoy::extensions::filters::http::acl::v3::Acl&) {}
};

using AclFilterGlobalConfigSharedPtr = std::shared_ptr<AclFilterGlobalConfig>;

// VH及路由配置
class AclFilterRouteConfig : public Router::RouteSpecificFilterConfig,
                             public Logger::Loggable<Logger::Id::filter> {
public:
  AclFilterRouteConfig(const envoy::extensions::filters::http::acl::v3::AclPerRoute&);

  // PB属性
public:
  bool enable() const { return enable_; }
  const std::vector<std::unique_ptr<Rule>>& rules() const { return rules_; }
  static bool compareRulesById(std::unique_ptr<Rule>& a, std::unique_ptr<Rule>& b);
  const std::string username() const { return username_; }

private:
  const bool enable_;
  const std::vector<std::unique_ptr<Rule>> rules_;
  std::string username_;
};

class AclFilter : public Http::PassThroughDecoderFilterEx,
                  public Logger::Loggable<Logger::Id::filter> {
public:
  AclFilter(AclFilterGlobalConfigSharedPtr config,
            const Server::Configuration::ServerFactoryContext& context)
      : PassThroughDecoderFilterEx(context), filter_hcm_config_(config) {}

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance&, bool end_stream) override;

  bool match(const AclFilterRouteConfig* filter_vh_config,
             const AclFilterRouteConfig* filter_route_config,
             const Network::Address::InstanceConstSharedPtr downstreamAddress,
             const Http::RequestHeaderMap& headers, const std::string& username);
  bool
  executeAction(const std::unique_ptr<Envoy::Extensions::HttpFilters::AclFilter::Rule>& match_rule,
                const Network::Address::InstanceConstSharedPtr downstreamAddress,
                const std::string& username);
  void setLog(const v3::AclLog_Action& action, const std::string& rule_name,
                         const Network::Address::InstanceConstSharedPtr downstreamAddress,
                         const std::string& username);
  void processDeny(const std::string& deny_text, const std::string& type);
  void onStreamComplete() override;
  v3::AclLog& getLog() { return log_; }

private:
  AclFilterGlobalConfigSharedPtr filter_hcm_config_;
  static const std::string filter_name_;
  v3::AclLog log_;
  Http::RequestHeaderMap* saved_headers_ = nullptr;

private:
  inline const Envoy::Router::VirtualHostImpl* getVirtualHost() const;
  inline const AclFilterRouteConfig*
  getVirtualHostConfig(const Envoy::Router::VirtualHostImpl* vh) const;
  inline const AclFilterRouteConfig*
  getRouteConfig(const AclFilterRouteConfig* filter_vh_config) const;
  inline const Network::Address::InstanceConstSharedPtr getDownstreamAddress() const;
  inline const std::string& getUpstreamName() const;
  inline bool getUserInfo(std::string& user_name) const;
};

} // namespace AclFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
