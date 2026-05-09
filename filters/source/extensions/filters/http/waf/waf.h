#pragma once

#include <string>

#include "source/extensions/filters/http/common/pass_through_filter_ex.h"

#include "source/common/common/logger.h"
#include "source/common/http/header_map_impl.h"

#include "envoy/thread_local/thread_local.h"
#include "envoy/http/filter.h"
#include "envoy/http/header_map.h"
#include "envoy/runtime/runtime.h"

#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"

#include "envoy/server/filter_config.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/protobuf/protobuf.h"

#include "webhook_fetcher.h"
#include "utility.h"

#include "filters/source/extensions/filters/http/strong_global_ratelimit/strong_global_ratelimit.h"
#include "filters/source/extensions/filters/http/common/ratelimit/impl/ratelimit_client_impl.h"
#include "filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3/strong_global_ratelimit.pb.h"
#include "filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3/strong_global_ratelimit_log.pb.h"
#include "filters/api/envoy/extensions/filters/http/waf/v3/waf.pb.h"
#include "filters/api/envoy/extensions/filters/http/waf/v3/waf_log.pb.h"

#include "modsecurity/modsecurity.h"
#include "modsecurity/rules_set.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WafFilter {

constexpr char filter_name[] = "envoy.filters.http.waf.1.0";

namespace ratelimit_v3 = envoy::extensions::filters::http::strong_global_ratelimit::v3;
using RatelimitFilterGlobalConfigSharedPtr =
    std::shared_ptr<StrongGlobalRatelimit::StrongGlobalRateLimitFilterGlobalConfig>;
using RatelimitFilterRouteConfigSharedPtr =
    std::shared_ptr<StrongGlobalRatelimit::StrongGlobalRateLimitFilterRouteConfig>;
using RatelimitFilterGlobalSharedPtr =
    std::shared_ptr<StrongGlobalRatelimit::StrongGlobalRateLimitFilter>;

#define ALL_MODSEC_STATS(COUNTER)                                                                  \
  COUNTER(request_processed)                                                                       \
  COUNTER(response_processed)

struct WafStats {
  ALL_MODSEC_STATS(GENERATE_COUNTER_STRUCT)
};

class FilterRouteConfig : public Router::RouteSpecificFilterConfig {
public:
  FilterRouteConfig(const v3::WafRoute& proto_config);
  bool disable_request() const { return disable_request_; }
  bool disable_response() const { return disable_response_; }
  // 限速 route
  bool enable_ratelimit() const { return enable_ratelimit_; }
  const std::vector<Filters::Common::IpWhitelist::IpWhitelistPtr>& white_list() const {
    return ip_whitelist_;
  }
  RatelimitFilterRouteConfigSharedPtr ratelimit_route_config() const {
    return ratelimit_route_config_;
  }
  void setRatelimitRouteProtoConfig(
      const v3::WafRoute& proto_config,
      ratelimit_v3::StrongGlobalRateLimitRoute& ratelimit_route_proto_config);
  const ratelimit_v3::StrongGlobalRateLimitRoute& ratelimit_route_proto_config() const {
    return ratelimit_route_proto_config_;
  }

private:
  const bool disable_request_;
  const bool disable_response_;
  const bool enable_ratelimit_;
  const std::vector<Filters::Common::IpWhitelist::IpWhitelistPtr> ip_whitelist_;
  RatelimitFilterRouteConfigSharedPtr ratelimit_route_config_;
  ratelimit_v3::StrongGlobalRateLimitRoute ratelimit_route_proto_config_;
};

class FilterGlobalConfig : public Logger::Loggable<Logger::Id::filter>,
                           public WebhookFetcherCallback {
public:
  FilterGlobalConfig(const v3::WafGlobal& proto_config, const std::string& stats_prefix,
                     Server::Configuration::FactoryContext&);
  ~FilterGlobalConfig();

  Runtime::Loader& runtime() { return runtime_; }
  WafStats& stats() { return stats_; }

  // rule info
  struct RuleInfo {
    std::string rule_id;
    uint32_t level;
  };
  // get config
  const std::string& rules_path_before() const { return rules_path_before_; }
  const std::string& rules_path_after() const { return rules_path_after_; }
  const std::string& tar_rules_path() const { return tar_rules_path_; }
  const v3::WafGlobal::Moderation& mode() const { return mode_; }
  const std::list<RuleInfo>& disable_rules() const { return disable_rules_; }
  const std::set<int64_t>& getDisableRulesId() const { return disable_rules_id_; }
  const std::list<RuleInfo>& pass_rules() const { return pass_rules_; }
  const std::set<int64_t>& getPassRulesId() const { return pass_rules_id_; }
  bool detectionOnly() const { return detection_only_; }
  // 限速golbal
  RatelimitFilterGlobalConfigSharedPtr ratelimit_global_config() const {
    return ratelimit_global_config_;
  }
  void setRatelimitGlobalProtoConfig(
      const v3::WafGlobal& proto_config,
      ratelimit_v3::StrongGlobalRateLimitGlobal& ratelimit_global_proto_config_);
  const ratelimit_v3::StrongGlobalRateLimitGlobal& ratelimit_global_proto_config() const {
    return ratelimit_global_proto_config_;
  }
  Filters::Common::RatelimitClient::ClientPtr makeClient() const { return makeClient_(); }

  const v3::WafGlobal::Webhook& webhook() const { return webhook_; }
  const v3::WafGlobal::ParanoiaLevel& paranoia_level() const { return paranoia_level_; }
  const uint64_t& ruleTypes() const { return rule_types_; }
  std::shared_ptr<modsecurity::ModSecurity> modsec() const { return modsec_; }
  std::shared_ptr<modsecurity::RulesSet> modsec_rules() const { return modsec_rules_; }

  void invoke_webhook(const modsecurity::RuleMessage*);

  // Webhook Callbacks
  void onSuccess(const Http::ResponseMessagePtr& response) override;
  void onFailure(FailureReason reason) override;
  // rule action
  void moderationOption(const v3::WafGlobal::Moderation mode_, std::string& rule_head,
                        std::string& rule_body);
  void runRuleAction(const char* const rule_cmd, const char* const option = "cmd");
  void setPassRuleAction(int phase, const std::string& rule_id, uint32_t level,
                         std::string& rule_cmd);
  int getPhaseById(int64_t rule_id);
  void getRealRuleId(std::list<RuleInfo>& rule_id);

private:
  // rule conf解密相关接口
  bool decryptWafConf(std::shared_ptr<char[]> passwd);
  std::shared_ptr<char[]> parseRuleConf();
  std::vector<std::string> restoreRulePath(std::shared_ptr<char[]> passwd);
  void removeRulePath(const std::vector<std::string>& rule_conf_path);

  // 设置仅检测模式
  void setDetectOnly();
  // 设置防护等级 硬编码
  void setDefendLevel();
  // 加载before conf
  void loadCoreRulesBefore();
  // 设置性能模式
  void setPerformanceMode();
  // 加载核心规则集
  void loadCoreRulesSet();
  // 加载after conf
  void loadCoreRulesAfter();
  // 不同防护等级重设分数
  void setLevelScore();
  // 设置放行的规则
  void setPassRules();
  // 设置禁用的规则
  void setDisableRules();

private:
  static WafStats generateStats(const std::string& prefix, Stats::Scope& scope) {
    return WafStats{ALL_MODSEC_STATS(POOL_COUNTER_PREFIX(scope, prefix))};
  }
  std::list<RuleInfo>
  generateRuleInfo(const google::protobuf::RepeatedPtrField<v3::WafGlobal_RuleInfo>& rules) {
    std::list<RuleInfo> tmp_info;
    RuleInfo tmp;
    for (int i = 0; i < rules.size(); i++) {
      tmp.rule_id = rules[i].id();
      tmp.level = rules[i].level();
      tmp_info.push_back(tmp);
    }
    return tmp_info;
  }

  struct ThreadLocalWebhook : public ThreadLocal::ThreadLocalObject {
    ThreadLocalWebhook(WebhookFetcher* webhook_fetcher) : webhook_fetcher_(webhook_fetcher) {}
    WebhookFetcherSharedPtr webhook_fetcher_;
  };

  // config data
  const static std::string conf_path_base_;
  const std::string rules_path_before_;
  const std::string rules_path_after_;
  const std::string tar_rules_path_;
  const v3::WafGlobal::Webhook webhook_;
  const v3::WafGlobal::ParanoiaLevel paranoia_level_;
  const bool detection_only_;
  const v3::WafGlobal::Moderation mode_;
  std::list<RuleInfo> disable_rules_;
  std::set<int64_t> disable_rules_id_;
  std::list<RuleInfo> pass_rules_;
  std::set<int64_t> pass_rules_id_;

  // 限速global
  RatelimitFilterGlobalConfigSharedPtr ratelimit_global_config_;
  ratelimit_v3::StrongGlobalRateLimitGlobal ratelimit_global_proto_config_;
  std::function<Filters::Common::RatelimitClient::ClientPtr()> makeClient_;
  Server::Configuration::FactoryContext& context_;

  ThreadLocal::SlotPtr tls_;

  WafStats stats_;
  Runtime::Loader& runtime_;
  uint64_t rule_types_;

  // share modsecurity obj
  std::shared_ptr<modsecurity::ModSecurity> modsec_;
  std::shared_ptr<modsecurity::RulesSet> modsec_rules_;
};

using FilterGlobalConfigSharedPtr = std::shared_ptr<FilterGlobalConfig>;

/**
 * Transaction flow:
 * 1. Disruptive?
 *   a. StopIterationAndBuffer until finished processing request
 *      a1. Should block? sendLocalReply
 *           decode should return StopIteration to avoid sending data to upstream.
 *           encode should return Continue to let local reply flow back to downstream.
 *      a2. Request is valid
 *           decode should return Continue to let request flow upstream.
 *           encode should return StopIterationAndBuffer until finished processing response
 *               a2a. Should block? goto a1.
 *               a2b. Response is valid, return Continue
 *
 * 2. Non-disruptive - always return Continue
 *
 */

class Filter : public Http::PassThroughFilterEx, public Logger::Loggable<Logger::Id::filter> {
public:
  /**
   * This static function will be called by modsecurity and internally invoke logCb filter's method
   */
  static void _logCb(void* data, const void* ruleMessagev);

  Filter(FilterGlobalConfigSharedPtr, Server::Configuration::ServerFactoryContext&);
  ~Filter() {}

  // Http::StreamFilterBase
  void onDestroy() override;
  void onStreamComplete() override;
  v3::WafLog& getLog() { return log_; }
  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance&, bool end_stream) override;
  Http::FilterTrailersStatus decodeTrailers(Http::RequestTrailerMap& trailers) override;

  // Http::StreamEncoderFilter
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool end_stream) override;
  Http::FilterDataStatus encodeData(Buffer::Instance&, bool end_stream) override;
  FilterGlobalConfigSharedPtr getConfig() { return config_; }

  // route config
  inline const Envoy::Router::VirtualHostImpl* getVirtualHost() const;
  virtual const FilterRouteConfig*
  getVirtualHostConfig(const Envoy::Router::VirtualHostImpl* vh) const;

  // 限速请求流设置，filter构造完成后，在addStreamFilter内部会调用此函数
  // void setDecoderFilterCallbacks(Http::StreamDecoderFilterCallbacks& callbacks) override {
  //   PassThroughFilterEx::setDecoderFilterCallbacks(callbacks);
  //   ratelimit_filter_->setDecoderFilterCallbacks(*(this->decoder_callbacks_));
  // }

  // waf处理request headers封装
  bool wafRequestHeadersProc(Http::RequestHeaderMap& headers, bool end_stream);
  // waf处理request data封装
  bool wafRequestDataProc(const Buffer::Instance& data, bool end_stream);
  // waf处理encode headers封装
  bool wafResponseHeadersProc(Http::ResponseHeaderMap& headers, bool end_stream);
  // waf处理encode data封装
  bool wafResponseDataProc(const Buffer::Instance& data, bool end_stream);
  // 限速可视化字段设置
  void setRatelimitVisual();
  // 设置可视化规则匹配详情
  void setRequestHeadPayload(std::pair<absl::string_view, absl::string_view> match_details,
                             v3::MatchPayload* match_payload);
  size_t getMatchDetailsPos(const std::string& decode_str, absl::string_view match_value,
                            absl::string_view orignal_info);
  std::string fromHexIfNeeded(const std::string& str);
  void setMatchPayload(v3::MatchPayload* match_payload, size_t start,
                       std::pair<absl::string_view, absl::string_view> match_details,
                       v3::PayloadStage stage);
  std::pair<absl::string_view, absl::string_view> getMatchRuleDetails(absl::string_view match_view);
  void setMatchRuleDetails(const modsecurity::RuleMessage* ruleMessage);
  void setRuleDetectStatus(const modsecurity::RuleMessage* ruleMessage);
  void setRuleVisual(const modsecurity::RuleMessage* ruleMessage);

private:
  enum class BlockStage {
    DecodeRequestHeader,
    DecodeRequestBody,
    EncodeResponseHeader,
    EncodeResponseBody,
    NoneBlock
  };
  BlockStage block_stage_{BlockStage::NoneBlock};
  const Buffer::Instance* decoding_buffer{};
  const Buffer::Instance* encoding_buffer{};

  v3::WafLog log_;
  const FilterGlobalConfigSharedPtr config_;
  std::shared_ptr<modsecurity::Transaction> modsec_transaction_;

  // 限速filter
  RatelimitFilterGlobalSharedPtr ratelimit_filter_;

  void logCb(const modsecurity::RuleMessage* ruleMessage);

  bool requestDisabled();
  bool responseDisabled();
  bool isWhiteList(const std::vector<Filters::Common::IpWhitelist::IpWhitelistPtr>& white_list,
                   const Network::Address::InstanceConstSharedPtr downstreamAddress);
  /**
   * @return true if intervention of current transaction is disruptive, false otherwise
   */
  bool intervention();
  Http::FilterHeadersStatus getRequestHeadersStatus();
  Http::FilterDataStatus getRequestStatus();

  Http::FilterHeadersStatus getResponseHeadersStatus();
  Http::FilterDataStatus getResponseStatus();

  struct WafStatus {
    WafStatus() : intervined(0), request_processed(0), response_processed(0) {}
    bool intervined;
    bool request_processed;
    bool response_processed;
  };

  // 白名单标记
  bool white_list_flag_;
  // 缓冲区标记
  bool is_decode_buffer_full_{false};
  bool is_encode_buffer_full_{false};

  WafStatus status_;
  // 用于去重攻击类型
  // std::set<std::string> attack_types_;
};

// 为在waf里实现限速路由配置的动态获取，这里继承限速filter
class WafStrongGlobalRateLimitFilter : public StrongGlobalRatelimit::StrongGlobalRateLimitFilter {
public:
  WafStrongGlobalRateLimitFilter(RatelimitFilterGlobalConfigSharedPtr config,
                                 Filters::Common::RatelimitClient::ClientPtr&& client,
                                 Server::Configuration::ServerFactoryContext& context)
      : StrongGlobalRatelimit::StrongGlobalRateLimitFilter(config, std::move(client), context) {}
  const StrongGlobalRatelimit::StrongGlobalRateLimitFilterRouteConfig*
  getRouteConfig() const override;
  void complete(Filters::Common::RatelimitClient::LimitStatus status,
                Filters::Common::RatelimitClient::LimitGrpcResponsePtr&& response) override;
  void setRequestHeaders(Http::RequestHeaderMap* headers) { request_headers_ = headers; }
  void setRequestStreamFlag(bool end_stream) { end_stream_ = end_stream; }
  void setWafFilter(Filter* const waf_filter_ptr) { waf_filter_ptr_ = waf_filter_ptr; }
  Http::RequestHeaderMap* getRequestHeaders() const { return request_headers_; }
  bool getRequestStreamFlag() const { return end_stream_; }
  Filter* getWafFilter() const { return waf_filter_ptr_; }

private:
  Filter* waf_filter_ptr_;
  Http::RequestHeaderMap* request_headers_{};
  bool end_stream_{false};
};

} // namespace WafFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy