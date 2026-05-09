#include <cmath>

#include "strong_local_ratelimit.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {

Event::TimerPtr FilterGlobalConfig::timer_;

const Http::LowerCaseString Filter::header_ratelimit_duration_("X-RateLimit-Duration");
const Http::LowerCaseString Filter::header_ratelimit_remaining_("X-RateLimit-Remaining");
const std::string Filter::filter_name_(FILTER_NAME);
const std::string Filter::srhino_domain_metadata_("srhino_metadata");
const std::string Filter::srhino_key_user_name_("user_name");

FilterGlobalConfig::FilterGlobalConfig(const v3::StrongLocalRateLimitGlobal&,
                                       Event::Dispatcher& dispatcher,
                                       Stats::Scope& must_be_server_scope)
    : dispatcher_(dispatcher), stats_(generateStats(must_be_server_scope)) {
  if (Impl::Quota::getStats() == nullptr) {
    Impl::Quota::setStats(&(stats_.quota_instance_count_));
  }

  if (timer_ == nullptr) {
    timer_ = dispatcher.createTimer([=]() { onTimer(); });
    timer_->enableTimer(std::chrono::milliseconds(LRU_CACHE_CLEAN_ELAPSE_MAX));
  }
}

void FilterGlobalConfig::onTimer() {
  // 清理LRU缓存
  Impl::Action::cleanQuotas(3);

  if (timer_) {
    timer_->enableTimer(std::chrono::milliseconds(calcCleanQuotasElapse()));
  }
}

uint32_t FilterGlobalConfig::calcCleanQuotasElapse() {
  static_assert(LRU_CACHE_CLEAN_ELAPSE_MAX >= 1,
                "the LRU_CACHE_CLEAN_ELAPSE_MAX macro must be greater than "
                "or equal to 1");
  static_assert(LRU_CACHE_CLEAN_ELAPSE_MAX >= LRU_CACHE_CLEAN_ELAPSE_MIN,
                "the LRU_CACHE_CLEAN_ELAPSE_MAX macro must be greater than or equal to "
                "LRU_CACHE_CLEAN_ELAPSE_MIN");

  constexpr uint32_t adjustElapse = LRU_CACHE_CLEAN_ELAPSE_MAX - LRU_CACHE_CLEAN_ELAPSE_MIN;

  // 当前缓存数量越多清理频率越快，在区间[LRU_CACHE_CLEAN_ELAPSE_MIN,LRU_CACHE_CLEAN_ELAPSE_MAX]线性浮动
  size_t max_size = Impl::Action::quotasMaxSize();
  max_size = max_size == 0 ? 1 : max_size;
  size_t cur_size = Impl::Action::quotasSize();
  float ratio = static_cast<float>(cur_size) / max_size;
  ratio = ratio > 1 ? 1 : ratio;
  uint32_t elapse = LRU_CACHE_CLEAN_ELAPSE_MAX - std::floor(adjustElapse * ratio);

  return elapse;
}

StrongLocalRateLimitStats FilterGlobalConfig::generateStats(Stats::Scope& scope) {
  const std::string final_prefix = "filters.http_strong_local_rate_limit";
  return {ALL_STRONG_LOCAL_RATE_LIMIT_STATS(POOL_COUNTER_PREFIX(scope, final_prefix),
                                            POOL_GAUGE_PREFIX(scope, final_prefix))};
}

FilterRouteConfig::FilterRouteConfig(const v3::StrongLocalRateLimitRoute& proto_config,
                                     Event::Dispatcher&)
    : rules_([&proto_config]() {
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

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers,
                                                bool end_stream) {
  const Envoy::Router::VirtualHostImpl* vh = getVirtualHost();
  // 获取Route配置
  filter_route_config_ = const_cast<FilterRouteConfig*>(getRouteConfig());
  if (filter_route_config_ == nullptr) {
    return Http::FilterHeadersStatus::Continue;
  }

  STAT_INC(Filter, enable);

  // 获取IP、路由、上游集群名等信息
  const std::string user_name = getUserName();
  // 当用户识别插件在header中未识别出username，且配置中存在用户名匹配时，判断是否存在请求体，
  // 如果没有请求体，则直接在deocodeHeaders中执行后续匹配逻辑，不用在decodeData中再次获取username
  if (!end_stream && user_name == EMPTY_STRING && filter_route_config_->hasUserNameMatch()) {
    request_headers_ = &headers;
    return Http::FilterHeadersStatus::Continue;
  }

  const Network::Address::InstanceConstSharedPtr downstreamAddress = getDownstreamAddress();
  const std::string_view vh_name = getVirtualHostName(vh);

  // 逐条匹配限速规则
  for (auto& rule : filter_route_config_->rules()) {
    if (!rule->match(user_name, downstreamAddress, headers)) {
      continue;
    }

    STAT_INC(Filter, match);

    Impl::Action::ProcResult result;
    rule->action().proc(downstreamAddress, headers, vh_name, 0, rule->hash(),
                        rule->action().dryrun(), result, user_name);
    if (result.pass) {
      STAT_INC(Filter, ok);

      if (!rule->action().procNextRule()) {
        break;
      }
    } else {
      log_.set_act(v3::StrongLocalRatelimitLog::LIMIT);
      log_.set_target(static_cast<v3::StrongLocalRatelimitLog::Target>(rule->action().target()));

      // 本地回复

      decoder_callbacks_->sendLocalReply(
          Http::Code::TooManyRequests, "",
          [&result](Http::HeaderMap& headers) {
            headers.setReferenceKey(header_ratelimit_duration_, std::to_string(result.duration));
            headers.setReferenceKey(header_ratelimit_remaining_, std::to_string(result.remaining));
          },
          absl::nullopt, "");

      return Http::FilterHeadersStatus::StopIteration;
    }
  }
  log_.set_act(v3::StrongLocalRatelimitLog::ALLOW); // 限流通过的都为全局
  log_.set_target(
      static_cast<v3::StrongLocalRatelimitLog::Target>(v3::Action::ALL)); // 限流通过的都为全局

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus Filter::decodeData(Buffer::Instance&, bool end_stream) {
  if (!request_headers_ || !end_stream) {
    return Http::FilterDataStatus::Continue;
  }

  const std::string user_name = getUserName();
  const Network::Address::InstanceConstSharedPtr downstreamAddress = getDownstreamAddress();
  const Envoy::Router::VirtualHostImpl* vh = getVirtualHost();
  const std::string_view vh_name = getVirtualHostName(vh);

  // 逐条匹配限速规则
  for (auto& rule : filter_route_config_->rules()) {
    if (!rule->match(user_name, downstreamAddress, *request_headers_)) {
      continue;
    }

    STAT_INC(Filter, match);

    Impl::Action::ProcResult result;
    rule->action().proc(downstreamAddress, *request_headers_, vh_name, 0, rule->hash(),
                        rule->action().dryrun(), result, user_name);
    if (result.pass) {
      STAT_INC(Filter, ok);

      if (!rule->action().procNextRule()) {
        break;
      }
    } else {
      log_.set_act(v3::StrongLocalRatelimitLog::LIMIT);
      log_.set_target(static_cast<v3::StrongLocalRatelimitLog::Target>(rule->action().target()));

      // 本地回复

      decoder_callbacks_->sendLocalReply(
          Http::Code::TooManyRequests, "",
          [&result](Http::HeaderMap& headers) {
            headers.setReferenceKey(header_ratelimit_duration_, std::to_string(result.duration));
            headers.setReferenceKey(header_ratelimit_remaining_, std::to_string(result.remaining));
          },
          absl::nullopt, "");

      return Http::FilterDataStatus::StopIterationAndBuffer;
    }
  }
  log_.set_act(v3::StrongLocalRatelimitLog::ALLOW); // 限流通过的都为全局
  log_.set_target(
      static_cast<v3::StrongLocalRatelimitLog::Target>(v3::Action::ALL)); // 限流通过的都为全局

  return Http::FilterDataStatus::Continue;
}

void Filter::onStreamComplete() {
  log(MessageUtil::getJsonStringFromMessageOrDie(log_, false, true));
}

inline const Envoy::Router::VirtualHostImpl* Filter::getVirtualHost() const {
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

inline const FilterRouteConfig*
Filter::getVirtualHostConfig(const Envoy::Router::VirtualHostImpl* vh) const {
  const FilterRouteConfig* filter_vh_config = nullptr;
  if (vh != nullptr) {
    auto config = vh->perFilterConfig(filter_name_);
    if (config != nullptr) {
      filter_vh_config = dynamic_cast<const FilterRouteConfig*>(config);
    }
  }

  return filter_vh_config;
}

inline const Network::Address::InstanceConstSharedPtr Filter::getDownstreamAddress() const {
  return decoder_callbacks_->streamInfo().downstreamAddressProvider().remoteAddress();
}

inline uint32_t Filter::getRouteId() const {
  uint32_t route_id = 0;
  const auto& metadata = decoder_callbacks_->streamInfo().route()->metadata().filter_metadata();
  const auto iter = metadata.find("route_id");
  if (iter != metadata.end()) {
    const auto iter2 = iter->second.fields().find("route_id");
    if (iter2 != iter->second.fields().end()) {
      route_id = iter2->second.number_value();
    }
  }

  return route_id;
}

inline const std::string& Filter::getUpstreamName() const {
  static const std::string empty;
  Upstream::ClusterInfoConstSharedPtr cluster =
      decoder_callbacks_->streamInfo().upstreamClusterInfo().has_value()
          ? decoder_callbacks_->streamInfo().upstreamClusterInfo().value()
          : nullptr;

  return cluster ? cluster->name() : empty;
}

inline std::string_view Filter::getVirtualHostName(const Envoy::Router::VirtualHostImpl* vh) const {
  if (vh == nullptr) {
    return std::string_view();
  }

  return std::string_view(reinterpret_cast<const char*>(vh->statName().data()),
                          vh->statName().dataSize());
}

/**
 * 获取Route配置，没有Route配置则使用VH配置，都不存在则返回空。
 */
inline const FilterRouteConfig* Filter::getRouteConfig() const {
  const FilterRouteConfig* filter_config = nullptr;
  auto route = decoder_callbacks_->route();
  if (route) {
    // 虚拟主机和路由级别同时存在时，mostSpecificPerFilterConfig优先返回路由级别的配置，路由级别不存在时则返回虚拟主机级别配置。
    filter_config =
        dynamic_cast<const FilterRouteConfig*>(route->mostSpecificPerFilterConfig(filter_name_));
  }
  return filter_config;
}

inline std::string Filter::getUserName() const {
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

} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy