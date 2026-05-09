#include "acl_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AclFilter {

const std::string AclFilter::filter_name_(FILTER_NAME);

Rule::Rule(const v3::Rule& rule)
    : matchers_([&rule]() {
        std::vector<Filters::Common::StrongMatcher::Matcher> result;
        for (const envoy::extensions::filters::http::common::strong_matcher::v3::Matcher& matcher :
             rule.match()) {
          result.emplace_back(matcher);
        }
        return result;
      }()),
      action_(rule.act()), id_(rule.id()), enable_(rule.enable()),
      deny_context_(rule.deny_context()), rule_name_(rule.rule_name()), type_(rule.type()) {}

bool Rule::match(const Network::Address::InstanceConstSharedPtr& address,
                 const Http::RequestHeaderMap& headers, const std::string& username) const {
  for (auto& matcher : matchers_) {
    if (!matcher.matchAll(headers, address, username)) {
      return false;
    }
  }
  return true;
}

AclFilterRouteConfig::AclFilterRouteConfig(const v3::AclPerRoute& proto_config)
    : enable_(proto_config.enable()), rules_([&proto_config]() {
        std::vector<std::unique_ptr<Rule>> rules;
        for (auto& rule : proto_config.rules()) {
          if (rule.enable()) {
            rules.emplace_back(std::make_unique<Rule>(rule));
          }
        }
        std::sort(rules.begin(), rules.end(), compareRulesById);
        return rules;
      }()) {
  for (auto& rule : rules_) {
    for (auto& match : rule->matcher()) {
      for (auto& user : match.getUserMatchers()) {
        username_ = user->username();
      }
    }
  }
}

bool AclFilterRouteConfig::compareRulesById(std::unique_ptr<Rule>& lhs,
                                            std::unique_ptr<Rule>& rhs) {
  return lhs->id() < rhs->id();
}

Http::FilterHeadersStatus AclFilter::decodeHeaders(Http::RequestHeaderMap& headers, bool end_stream) {
  // 判断VH中是否存在配置
  const Envoy::Router::VirtualHostImpl* vh = getVirtualHost();
  const AclFilterRouteConfig* filter_vh_config = getVirtualHostConfig(vh);

  // 获取路由配置
  const AclFilterRouteConfig* filter_route_config = getRouteConfig(filter_vh_config);
  // 获取IP、上游集群名
  const Network::Address::InstanceConstSharedPtr downstreamAddress = getDownstreamAddress();
  std::string username;
  // 判断是否存在用户匹配
  if (filter_route_config != nullptr) {
    if (!filter_route_config->username().empty()) {
      if (!getUserInfo(username) && !end_stream) {
          // 保存请求头
          saved_headers_ = &headers;
          ENVOY_LOG(debug, "not found username");
          return Http::FilterHeadersStatus::Continue;
      }
    }
  } else if (filter_vh_config != nullptr) {
    if (!filter_vh_config->username().empty()) {
      if (!getUserInfo(username) && !end_stream) {
          saved_headers_ = &headers;
          ENVOY_LOG(debug, "not found username");
          return Http::FilterHeadersStatus::Continue;
      }
    }
  }

  if (!match(filter_vh_config, filter_route_config, downstreamAddress, headers, username)) {
    return Http::FilterHeadersStatus::StopIteration;
  }
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus AclFilter::decodeData(Buffer::Instance&, bool) {
  // 判断请求头是否存在
  if (saved_headers_ == nullptr) {
    return Http::FilterDataStatus::Continue;
  }
  std::string username;
  if (!getUserInfo(username)) {
    ENVOY_LOG(debug, "not found username");
  }
  // 判断VH中是否存在配置
  const Envoy::Router::VirtualHostImpl* vh = getVirtualHost();
  const AclFilterRouteConfig* filter_vh_config = getVirtualHostConfig(vh);

  // 获取路由配置
  const AclFilterRouteConfig* filter_route_config = getRouteConfig(filter_vh_config);
  // 获取IP、上游集群名
  const Network::Address::InstanceConstSharedPtr downstreamAddress = getDownstreamAddress();

  if (!match(filter_vh_config, filter_route_config, downstreamAddress, *saved_headers_, username)) {
    return Http::FilterDataStatus::StopIterationAndBuffer;
  }

  return Http::FilterDataStatus::Continue;
}

bool AclFilter::match(const AclFilterRouteConfig* filter_vh_config,
                      const AclFilterRouteConfig* filter_route_config,
                      const Network::Address::InstanceConstSharedPtr downstreamAddress,
                      const Http::RequestHeaderMap& headers, const std::string& username) {
  if (filter_route_config != nullptr) {
    if (filter_route_config->enable()) {
      const auto& route_rules = filter_route_config->rules();
      if (route_rules.size() > 0) {
        size_t route_id = route_rules.size() - 1;
        for (size_t id = 0; id < route_rules.size() - 1; id++) {
          if (route_rules[id]->match(downstreamAddress, headers, username)) {
            route_id = id;
            break;
          }
        }
        if (route_rules[route_id]->action() != v3::Action::ALLOW) {
          return executeAction(route_rules[route_id], downstreamAddress, username);
        }
      }
    }
  }

  if (filter_vh_config != nullptr) {
    const auto& vh_rules = filter_vh_config->rules();
    if (vh_rules.size() > 0) {
      size_t vh_id = vh_rules.size() - 1;
      for (size_t id = 0; id < vh_rules.size() - 1; id++) {
        if (vh_rules[id]->match(downstreamAddress, headers, username)) {
          vh_id = id;
          break;
        }
      }
      return executeAction(vh_rules[vh_id], downstreamAddress, username);
    }
  }
  return true;
}

// 返回true为继续，false为停止
bool AclFilter::executeAction(
    const std::unique_ptr<Envoy::Extensions::HttpFilters::AclFilter::Rule>& match_rule,
    const Network::Address::InstanceConstSharedPtr downstreamAddress, const std::string& username) {
  // 处理拒绝
  if (match_rule->action() == v3::Action::DENY) {
    setLog(v3::AclLog_Action_DENY, match_rule->rule_name(), downstreamAddress, username);
    const char* type = "text/plain;charset=UTF-8";
    if (match_rule->type() == "json") {
      type = "application/json;charset=UTF-8";
    } else if (match_rule->type() == "html") {
      type = "text/html;charset=UTF-8";
    }
    processDeny(match_rule->deny_context(), type);
    return false;
  } else if (match_rule->action() == v3::Action::RECORD) {
    setLog(v3::AclLog_Action_RECORD, match_rule->rule_name(), downstreamAddress, username);
    return true;
  }
  setLog(v3::AclLog_Action_ALLOW, match_rule->rule_name(), downstreamAddress, username);
  return true;
}

void AclFilter::setLog(const v3::AclLog_Action& action, const std::string& rule_name,
                       const Network::Address::InstanceConstSharedPtr downstreamAddress,
                       const std::string& username) {
  log_.set_act(action);
  log_.set_downstream(downstreamAddress->asString());
  log_.set_rule_name(rule_name);
  log_.set_username(username);
}

void AclFilter::processDeny(const std::string& deny_text, const std::string& type) {
  std::function<void(Http::ResponseHeaderMap&)> modify_headers_cb =
      [&](Http::ResponseHeaderMap& headers) { headers.setContentType(type); };
  decoder_callbacks_->sendLocalReply(Http::Code::Forbidden, deny_text, modify_headers_cb,
                                     absl::nullopt, absl::string_view());
}

void AclFilter::onStreamComplete() {
  log(MessageUtil::getJsonStringFromMessageOrDie(log_, false, true));
}

inline const Envoy::Router::VirtualHostImpl* AclFilter::getVirtualHost() const {
  auto route = decoder_callbacks_->route();
  if (route) {
    auto entry = route->routeEntry();
    if (entry) {
      auto& vh = entry->virtualHost();
      return dynamic_cast<const Envoy::Router::VirtualHostImpl*>(&vh);
    } else {
      auto route_impl = std::dynamic_pointer_cast<const Envoy::Router::RouteEntryImplBase>(route);
      if (route_impl && route_impl->isDirectResponse()) {
        const Envoy::Router::VirtualHost& vh = route_impl->virtualHost();
        return dynamic_cast<const Envoy::Router::VirtualHostImpl*>(&vh);
      }
    }
  }

  return nullptr;
}

inline const AclFilterRouteConfig*
AclFilter::getVirtualHostConfig(const Envoy::Router::VirtualHostImpl* vh) const {
  const AclFilterRouteConfig* filter_vh_config = nullptr;
  if (vh != nullptr) {
    auto config = vh->perFilterConfig(filter_name_);
    if (config != nullptr) {
      filter_vh_config = dynamic_cast<const AclFilterRouteConfig*>(config);
    }
  }

  return filter_vh_config;
}

inline const AclFilterRouteConfig*
AclFilter::getRouteConfig(const AclFilterRouteConfig* filter_vh_config) const {
  const AclFilterRouteConfig* filter_route_config = nullptr;
  auto route = decoder_callbacks_->route();
  if (route) {
    filter_route_config =
        dynamic_cast<const AclFilterRouteConfig*>(route->mostSpecificPerFilterConfig(filter_name_));
  }

  // 路由级别的配置不存在时，mostSpecificPerFilterConfig返回的是VH级别的配置
  return filter_route_config == filter_vh_config ? nullptr : filter_route_config;
}

inline const Network::Address::InstanceConstSharedPtr AclFilter::getDownstreamAddress() const {
  return decoder_callbacks_->streamInfo().downstreamAddressProvider().remoteAddress();
}

inline const std::string& AclFilter::getUpstreamName() const {
  static const std::string empty;
  Upstream::ClusterInfoConstSharedPtr cluster =
      decoder_callbacks_->streamInfo().upstreamClusterInfo().has_value()
          ? decoder_callbacks_->streamInfo().upstreamClusterInfo().value()
          : nullptr;

  return cluster ? cluster->name() : empty;
}

inline bool AclFilter::getUserInfo(std::string& user_name) const {
  auto& metadata = decoder_callbacks_->streamInfo().dynamicMetadata().filter_metadata();
  if (!metadata.contains(SrhinoDomainMetaData)) {
    return false;
  }

  auto& filter_meta = metadata.at(SrhinoDomainMetaData);
  auto& fields = filter_meta.fields();
  if (fields.contains(SrhinoKeyUserName)) {
    user_name = fields.at(SrhinoKeyUserName).string_value();
    return true;
  }
  return false;
}

} // namespace AclFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
