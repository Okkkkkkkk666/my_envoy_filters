#pragma once

#include "envoy/event/timer.h"
#include "source/extensions/filters/http/common/pass_through_filter_ex.h"

#include "source/common/router/config_impl.h"

#include "filters/api/envoy/extensions/filters/http/strong_local_ratelimit/v3/strong_local_ratelimit.pb.h"
#include "filters/api/envoy/extensions/filters/http/strong_local_ratelimit/v3/strong_local_ratelimit_log.pb.h"

#include "test_stat.h"
#include "impl/rule.h"

#define FILTER_NAME "envoy.filters.http.strong-local-ratelimit.1.0"
#define LRU_CACHE_CLEAN_ELAPSE_MAX 1000 * 30
#define LRU_CACHE_CLEAN_ELAPSE_MIN 1000 * 3

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {

namespace v3 = envoy::extensions::filters::http::strong_local_ratelimit::v3;

#define ALL_STRONG_LOCAL_RATE_LIMIT_STATS(COUNTER, GAUGE) GAUGE(quota_instance_count, NeverImport)

// 统计信息
struct StrongLocalRateLimitStats {
  ALL_STRONG_LOCAL_RATE_LIMIT_STATS(GENERATE_COUNTER_STRUCT, GENERATE_GAUGE_STRUCT)
};

// 全局配置
class FilterGlobalConfig : public Router::RouteSpecificFilterConfig {
  friend class FilterTest;
  friend class ConfigTest;

public:
  FilterGlobalConfig(const v3::StrongLocalRateLimitGlobal& proto_config,
                     Event::Dispatcher& dispatcher, Stats::Scope& must_be_server_scope);

public:
  StrongLocalRateLimitStats& stats() const { return stats_; }

private:
  Event::Dispatcher& dispatcher_;

  // admin 统计信息
  mutable StrongLocalRateLimitStats stats_;

  // 清理lru定时器
private:
  static Event::TimerPtr timer_;

private:
  static void onTimer();
  // 动态调整清理缓存频率
  static uint32_t calcCleanQuotasElapse();
  // 初始化统计信息
  static StrongLocalRateLimitStats generateStats(Stats::Scope& scope);
};

using FilterGlobalConfigSharedPtr = std::shared_ptr<FilterGlobalConfig>;

// VH配置
class FilterRouteConfig : public Router::RouteSpecificFilterConfig {
  friend class ConfigTest;
  friend class FilterTest;
  friend class ActionTest;

public:
  FilterRouteConfig(const v3::StrongLocalRateLimitRoute& proto_config,
                    Event::Dispatcher& dispatcher);

public:
  const std::vector<Impl::RulePtr>& rules() const { return rules_; }
  bool hasUserNameMatch() const { return match_username_; }

private:
  // 限速规则
  const std::vector<Impl::RulePtr> rules_;
  // 是否进行用户名匹配
  bool match_username_{false};
};

using FilterRouteConfigSharedPtr = std::shared_ptr<FilterGlobalConfig>;

class Filter : public Http::PassThroughFilterEx {
public:
  Filter(FilterGlobalConfigSharedPtr config,
         const Server::Configuration::ServerFactoryContext& context)
      : Http::PassThroughFilterEx(context), filter_hcm_config_(config) {}

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance& data, bool end_stream) override;

  void onStreamComplete() override;
  v3::StrongLocalRatelimitLog& getLog() { return log_; }

private:
  FilterGlobalConfigSharedPtr filter_hcm_config_;
  static const Http::LowerCaseString header_ratelimit_duration_;
  static const Http::LowerCaseString header_ratelimit_remaining_;
  // 防止调用perFilterConfig时每次都构造string，这里提前构造
  static const std::string filter_name_;
  static const std::string srhino_domain_metadata_;
  static const std::string srhino_key_user_name_;
  FilterRouteConfig* filter_route_config_;
  Http::RequestHeaderMap* request_headers_{NULL};

private:
  inline const Envoy::Router::VirtualHostImpl* getVirtualHost() const;
  inline const FilterRouteConfig*
  getVirtualHostConfig(const Envoy::Router::VirtualHostImpl* vh) const;
  inline const Network::Address::InstanceConstSharedPtr getDownstreamAddress() const;
  inline uint32_t getRouteId() const;
  inline const std::string& getUpstreamName() const;
  inline std::string_view getVirtualHostName(const Envoy::Router::VirtualHostImpl* vh) const;
  inline const FilterRouteConfig* getRouteConfig() const;
  inline std::string getUserName() const;

  v3::StrongLocalRatelimitLog log_;
  // 用于单元测试的统计信息
public:
  DECLARE_STAT_BEGIN(Filter)
  STAT(enable)
  STAT(match)
  STAT(ok)
  DECLARE_STAT_END(Filter)
};
} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
