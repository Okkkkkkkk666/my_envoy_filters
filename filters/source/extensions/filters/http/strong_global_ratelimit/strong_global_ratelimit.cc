#include "strong_global_ratelimit.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongGlobalRatelimit {

const Http::LowerCaseString
    StrongGlobalRateLimitFilter::header_ratelimit_duration_("X-RateLimit-Duration");
const Http::LowerCaseString
    StrongGlobalRateLimitFilter::header_ratelimit_remaining_("X-RateLimit-Remaining");
const Http::LowerCaseString
    StrongGlobalRateLimitFilter::header_ratelimit_drop_("X-RateLimit-Limit");
const std::string StrongGlobalRateLimitFilter::filter_name_(FILTER_NAME);
const uint16_t StrongGlobalRateLimitFilter::zero_number_(0);
const std::string StrongGlobalRateLimitFilter::srhino_domain_metadata_("srhino_metadata");
const std::string StrongGlobalRateLimitFilter::srhino_key_user_name_("user_name");

StrongGlobalRatelimit::StrongGlobalRateLimitFilterRouteConfig::
    StrongGlobalRateLimitFilterRouteConfig(const v3::StrongGlobalRateLimitRoute& proto_config)
    : external_invoke_(proto_config.external_invoke()), rules_([&proto_config]() {
        std::vector<Impl::RulePtr> rules;
        for (const auto& rule : proto_config.rules()) {
          if (rule.enable()) {
            rules.push_back(std::make_unique<Impl::Rule>(rule));
          }
        }
        return rules;
      }()) {
  for (const auto& rule : rules_) {
    if (rule->hasUserNameMatch()) {
      match_username_ = true;
    }
  }
}

Http::FilterHeadersStatus
StrongGlobalRateLimitFilter::decodeHeaders(Http::RequestHeaderMap& headers, bool end_stream) {

  request_headers_ = &headers;

  // 获取Route配置
  filter_route_config_ = const_cast<StrongGlobalRateLimitFilterRouteConfig*>(getRouteConfig());
  if (filter_route_config_ == nullptr) {
    return Http::FilterHeadersStatus::Continue;
  }

  // 获取限速是否外部插件调用
  external_invoke_ = filter_route_config_->externalInvoke();

  // 获取用户名、虚拟主机名等信息
  const std::string user_name = getUserName();
  // 当用户识别插件在header中未识别出username，且配置中存在用户名匹配时，判断是否存在请求体，
  // 如果没有请求体，则直接在deocodeHeaders中执行后续匹配逻辑，不用在decodeData中再次获取username
  if (!end_stream && user_name == EMPTY_STRING && filter_route_config_->hasUserNameMatch()) {
    state_ = State::NeedDecodeData;
    return Http::FilterHeadersStatus::Continue;
  }
  log_.set_user_name(user_name);

  const std::string vh_name = getVirtualHostName(context_, decoder_callbacks_->streamInfo());
  const Network::Address::InstanceConstSharedPtr downstreamAddress = getDownstreamAddress();
  // 逐条匹配限速规则
  for (auto& rule : filter_route_config_->rules()) {
    if (!rule->match(headers, downstreamAddress, user_name)) {
      continue;
    }
    Filters::Common::RatelimitClient::Impl::Quotas quotas = rule->action().getQuotas();
    if (quotas[0].duration_ == 0 && quotas[0].max_count_ == 0) {
      continue;
    }
    const std::string* filter_instance_name = &EMPTY_STRING;
    if (filter_hcm_config_->getFilterInstanceName().empty()) {
      filter_instance_name = &filter_hcm_config_->setFilterInstanceName(
          getFilterInstanceName(this->getFilterAccessLogKey()));
    } else {
      filter_instance_name = &filter_hcm_config_->getFilterInstanceName();
    }
    std::string key = rule->action().makeKey(downstreamAddress, headers, vh_name, rule->hash(),
                                             *filter_instance_name, user_name);
    ratelimit_policys_.emplace_back(
        std::make_shared<Filters::Common::RatelimitClient::Impl::RateLimitPolicy>(
            key, quotas, zero_number_, rule->ruleName(), false, rule->action().dryrun()));
    if (!rule->action().procNextRule()) {
      break;
    }
  }
  if (!ratelimit_policys_.empty()) {
    state_ = State::Calling;
    client_->limit(*this, ratelimit_policys_, decoder_callbacks_->activeSpan(),
                   decoder_callbacks_->streamInfo());
  }

  return (state_ == State::Calling || state_ == State::Responded)
             ? Http::FilterHeadersStatus::StopIteration
             : Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus StrongGlobalRateLimitFilter::decodeData(Buffer::Instance&, bool end_stream) {
  ASSERT(state_ != State::Responded);
  if (state_ == State::Calling) {
    return Http::FilterDataStatus::StopIterationAndWatermark;
  }

  if (state_ == State::NeedDecodeData && end_stream) {
    const std::string user_name = getUserName();
    log_.set_user_name(user_name);
    const std::string vh_name = getVirtualHostName(context_, decoder_callbacks_->streamInfo());
    const Network::Address::InstanceConstSharedPtr downstreamAddress = getDownstreamAddress();
    // 逐条匹配限速规则
    for (auto& rule : filter_route_config_->rules()) {
      if (!rule->match(*request_headers_, downstreamAddress, user_name)) {
        continue;
      }
      Filters::Common::RatelimitClient::Impl::Quotas quotas = rule->action().getQuotas();
      if (quotas[0].duration_ == 0 && quotas[0].max_count_ == 0) {
        continue;
      }
      const std::string* filter_instance_name = &EMPTY_STRING;
      if (filter_hcm_config_->getFilterInstanceName().empty()) {
        filter_instance_name = &filter_hcm_config_->setFilterInstanceName(
            getFilterInstanceName(this->getFilterAccessLogKey()));
      } else {
        filter_instance_name = &filter_hcm_config_->getFilterInstanceName();
      }
      std::string key = rule->action().makeKey(downstreamAddress, *request_headers_, vh_name,
                                               rule->hash(), *filter_instance_name, user_name);
      ratelimit_policys_.emplace_back(
          std::make_shared<Filters::Common::RatelimitClient::Impl::RateLimitPolicy>(
              key, quotas, zero_number_, rule->ruleName(), false, rule->action().dryrun()));
      if (!rule->action().procNextRule()) {
        break;
      }
    }
    if (!ratelimit_policys_.empty()) {
      state_ = State::Calling;
      client_->limit(*this, ratelimit_policys_, decoder_callbacks_->activeSpan(),
                     decoder_callbacks_->streamInfo());
    }
    return (state_ == State::Calling || state_ == State::Responded)
               ? Http::FilterDataStatus::StopIterationAndWatermark
               : Http::FilterDataStatus::Continue;
  }

  return Http::FilterDataStatus::Continue;
}

Http::FilterTrailersStatus StrongGlobalRateLimitFilter::decodeTrailers(Http::RequestTrailerMap&) {
  ASSERT(state_ != State::Responded);
  return state_ == State::Calling ? Http::FilterTrailersStatus::StopIteration
                                  : Http::FilterTrailersStatus::Continue;
}

void StrongGlobalRateLimitFilter::onDestroy() {
  if (state_ == State::Calling) {
    state_ = State::Complete;
    client_->cancel();
  }
}

void StrongGlobalRateLimitFilter::onStreamComplete() {
  if (is_push_log_) {
    log(MessageUtil::getJsonStringFromMessageOrDie(log_, false, true));
  }
}

void StrongGlobalRateLimitFilter::complete(
    Filters::Common::RatelimitClient::LimitStatus status,
    Filters::Common::RatelimitClient::LimitGrpcResponsePtr&& response) {
  state_ = State::Complete;
  switch (status) {
  case Filters::Common::RatelimitClient::LimitStatus::OverLimit: {
    setLog(std::move(response));

    // 是否空转
    if (response->dryrun()) {
      request_headers_->setReferenceKey(header_ratelimit_drop_, "drop");
      log_.set_act(v3::StrongGlobalRatelimitLog_Act_DRYRUN);
      decoder_callbacks_->continueDecoding();
    } else {
      state_ = State::Responded;
      log_.set_act(v3::StrongGlobalRatelimitLog_Act_REFUSE);
      decoder_callbacks_->sendLocalReply(
          Http::Code::TooManyRequests, "",
          [&response](Http::HeaderMap& headers) {
            headers.setReferenceKey(header_ratelimit_duration_,
                                    std::to_string(response->duration()));
            headers.setReferenceKey(header_ratelimit_remaining_,
                                    std::to_string(response->remaining()));
          },
          absl::nullopt, "request_rate_limited");
    }
    break;
  }
  case Filters::Common::RatelimitClient::LimitStatus::Error: {
    state_ = State::NotSendRequest;
    decoder_callbacks_->sendLocalReply(Http::Code::InternalServerError, "", nullptr, absl::nullopt,
                                       "rate_limiter_error");
    break;
  }
  case Filters::Common::RatelimitClient::LimitStatus::OK: {
    if (!external_invoke_) {
      log_.set_act(v3::StrongGlobalRatelimitLog_Act_PASS);
      is_push_log_ = true;
      decoder_callbacks_->continueDecoding();
    }
    break;
  }
  }
}

inline const Envoy::Router::VirtualHostImpl* StrongGlobalRateLimitFilter::getVirtualHost() const {
  auto route = decoder_callbacks_->streamInfo().route();
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

/**
 * 获取Route配置，没有Route配置则使用VH配置，都不存在则返回空。
 */
const StrongGlobalRateLimitFilterRouteConfig* StrongGlobalRateLimitFilter::getRouteConfig() const {
  const StrongGlobalRateLimitFilterRouteConfig* filter_config = nullptr;
  auto route = decoder_callbacks_->route();
  if (route) {
    // 虚拟主机和路由级别同时存在时，mostSpecificPerFilterConfig优先返回路由级别的配置，路由级别不存在时则返回虚拟主机级别配置。
    filter_config = dynamic_cast<const StrongGlobalRateLimitFilterRouteConfig*>(
        route->mostSpecificPerFilterConfig(filter_name_));
  }
  return filter_config;
}

inline const Network::Address::InstanceConstSharedPtr
StrongGlobalRateLimitFilter::getDownstreamAddress() const {
  return decoder_callbacks_->streamInfo().downstreamAddressProvider().remoteAddress();
}

inline std::string StrongGlobalRateLimitFilter::getUserName() const {
  const auto& metadata = decoder_callbacks_->streamInfo().dynamicMetadata().filter_metadata();
  const auto iter = metadata.find(srhino_domain_metadata_);
  if (iter != metadata.end()) {
    const auto iter2 = iter->second.fields().find(srhino_key_user_name_);
    if (iter2 != iter->second.fields().end()) {
      return iter2->second.string_value();
    }
  }

  return EMPTY_STRING;
}

inline const std::string& StrongGlobalRateLimitFilter::getUpstreamName() const {
  static const std::string empty;
  Upstream::ClusterInfoConstSharedPtr cluster =
      decoder_callbacks_->streamInfo().upstreamClusterInfo().has_value()
          ? decoder_callbacks_->streamInfo().upstreamClusterInfo().value()
          : nullptr;

  return cluster ? cluster->name() : empty;
}

inline std::string StrongGlobalRateLimitFilter::getVirtualHostName(
    const Server::Configuration::ServerFactoryContext& context,
    const StreamInfo::StreamInfo& stream_info) const {
  auto route = stream_info.route();
  if (route) {
    auto entry = route->routeEntry();
    if (entry) {
      auto& vh = entry->virtualHost();
      return const_cast<Server::Configuration::ServerFactoryContext&>(context)
          .scope()
          .symbolTable()
          .toString(vh.statName());
    } else {
      auto route_impl = std::dynamic_pointer_cast<const Envoy::Router::RouteEntryImplBase>(route);
      if (route_impl && route_impl->isDirectResponse()) {
        const Envoy::Router::VirtualHost& vh = route_impl->virtualHost();
        return const_cast<Server::Configuration::ServerFactoryContext&>(context)
            .scope()
            .symbolTable()
            .toString(vh.statName());
      }
    }
  }

  return std::string();
}

inline const std::string&
StrongGlobalRateLimitFilter::getUpstreamHost(const std::string& cluster_name) const {
  static const std::string empty;
  auto cluster = const_cast<Server::Configuration::ServerFactoryContext&>(context_)
                     .clusterManager()
                     .getThreadLocalCluster(cluster_name);
  if (cluster == nullptr) {
    return empty;
  }
  const auto& host = cluster->loadBalancer().peekAnotherHost(nullptr);
  return host ? host->address()->asString() : empty;
}

std::string
StrongGlobalRateLimitFilter::getFilterInstanceName(const std::string& filter_access_log_key) const {
  auto last_open_bracket = filter_access_log_key.find_last_of("[");
  auto last_close_bracket = filter_access_log_key.find_last_of("]");
  if (last_open_bracket != std::string::npos && last_close_bracket != std::string::npos &&
      last_close_bracket > last_open_bracket) {
    return filter_access_log_key.substr(last_open_bracket + 1,
                                        last_close_bracket - last_open_bracket - 1);
  }

  return EMPTY_STRING;
}

inline void StrongGlobalRateLimitFilter::setLog(
    Filters::Common::RatelimitClient::LimitGrpcResponsePtr&& response) {
  is_push_log_ = true;
  for (const auto& rule : filter_route_config_->rules()) {
    log_.set_rule_name(response->rule_name());
    if (rule->ruleName() != response->rule_name()) {
      continue;
    }

    log_.set_target(rule->action().target());
    v3::Quota* new_quota = log_.mutable_quota();
    new_quota->set_duration(response->duration());
    for (const auto& quota : rule->action().getQuotas()) {
      if (quota.duration_ != response->duration()) {
        continue;
      }

      new_quota->set_max_count(quota.max_count_);
      break;
    }
    break;
  }
}

} // namespace StrongGlobalRatelimit
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
