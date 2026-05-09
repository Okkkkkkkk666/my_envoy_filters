#pragma once

#include "source/common/common/logger.h"
#include "source/common/router/config_impl.h"
#include "source/extensions/filters/http/common/pass_through_filter_ex.h"

#include "filters/source/extensions/filters/http/common/ratelimit/ratelimit_client.h"
#include "filters/source/extensions/filters/http/common/ip_whitelist/ip_whitelist.h"
#include "filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3/strong_global_ratelimit.pb.h"
#include "filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3/strong_global_ratelimit_log.pb.h"

#include "impl/rule.h"

#define FILTER_NAME "envoy.filters.http.strong-global-ratelimit.1.0"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongGlobalRatelimit {

namespace v3 = envoy::extensions::filters::http::strong_global_ratelimit::v3;

// 全局配置
class StrongGlobalRateLimitFilterGlobalConfig : public Router::RouteSpecificFilterConfig {
public:
  StrongGlobalRateLimitFilterGlobalConfig(const v3::StrongGlobalRateLimitGlobal&) {}
  const std::string& getFilterInstanceName() const { return filter_instance_name_; }

  const std::string& setFilterInstanceName(const absl::string_view& filter_instance_name) {
    filter_instance_name_.assign(filter_instance_name.data(), filter_instance_name.length());
    return filter_instance_name_;
  }

private:
  std::string filter_instance_name_;
};
using FilterGlobalConfigSharedPtr = std::shared_ptr<StrongGlobalRateLimitFilterGlobalConfig>;

// VH及路由配置
class StrongGlobalRateLimitFilterRouteConfig : public Router::RouteSpecificFilterConfig {
public:
  StrongGlobalRateLimitFilterRouteConfig(const v3::StrongGlobalRateLimitRoute& proto_config);

public:
  bool externalInvoke() const { return external_invoke_; }
  const std::vector<Impl::RulePtr>& rules() const { return rules_; }
  bool hasUserNameMatch() const {return match_username_;}

private:
  // 是否外部插件调用本插件
  const bool external_invoke_;
  // 限速规则
  const std::vector<Impl::RulePtr> rules_;
  // 是否进行用户名匹配
  bool match_username_{false};
};

class StrongGlobalRateLimitFilter : public Http::PassThroughFilterEx,
                                    public Filters::Common::RatelimitClient::LimitRequestCallbacks,
                                    public Logger::Loggable<Logger::Id::filter> {
public:
  StrongGlobalRateLimitFilter(FilterGlobalConfigSharedPtr config,
                              Filters::Common::RatelimitClient::ClientPtr&& client,
                              Server::Configuration::ServerFactoryContext& context)
      : Http::PassThroughFilterEx(context), filter_hcm_config_(config), client_(std::move(client)),
        context_(context) {}

  // Http::PassThroughFilterEx
  void onDestroy() override;
  void onStreamComplete() override;
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance& data, bool end_stream) override;
  Http::FilterTrailersStatus decodeTrailers(Http::RequestTrailerMap& trailers) override;

  // Filters::Common::RatelimitClient::RequestCallbacks
  void complete(Filters::Common::RatelimitClient::LimitStatus status,
                Filters::Common::RatelimitClient::LimitGrpcResponsePtr&& response) override;
  virtual const StrongGlobalRateLimitFilterRouteConfig* getRouteConfig() const;

private:
  inline const Envoy::Router::VirtualHostImpl* getVirtualHost() const;
  inline const Network::Address::InstanceConstSharedPtr getDownstreamAddress() const;
  inline std::string getUserName() const;
  inline const std::string& getUpstreamName() const;
  inline std::string getVirtualHostName(const Server::Configuration::ServerFactoryContext& context,
                                        const StreamInfo::StreamInfo& stream_info) const;
  inline const std::string& getUpstreamHost(const std::string& cluster_name) const;
  std::string getFilterInstanceName(const std::string& filter_access_log_key) const;
  inline void setLog(Filters::Common::RatelimitClient::LimitGrpcResponsePtr&& response);

public:
  enum class State { NotStarted, NeedDecodeData, Calling, Complete, Responded, NotSendRequest };
  inline State ratelimitState() const { return state_; }

private:
  FilterGlobalConfigSharedPtr filter_hcm_config_;
  Filters::Common::RatelimitClient::ClientPtr client_;
  const Server::Configuration::ServerFactoryContext& context_;
  State state_{State::NotStarted};
  static const std::string filter_name_;
  static const Http::LowerCaseString header_ratelimit_duration_;
  static const Http::LowerCaseString header_ratelimit_remaining_;
  static const Http::LowerCaseString header_ratelimit_drop_;
  static const uint16_t zero_number_;
  static const std::string srhino_domain_metadata_;
  static const std::string srhino_key_user_name_;
  std::vector<std::shared_ptr<Filters::Common::RatelimitClient::Impl::RateLimitPolicy>> ratelimit_policys_{};
  bool dryrun_{};
  bool external_invoke_{};
  Http::RequestHeaderMap* request_headers_{};
  StrongGlobalRateLimitFilterRouteConfig* filter_route_config_;
  v3::StrongGlobalRatelimitLog log_;
  bool is_push_log_{false};
};

} // namespace StrongGlobalRatelimit
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
