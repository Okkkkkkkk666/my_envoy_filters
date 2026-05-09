#pragma once

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "envoy/upstream/cluster_manager.h"
#include "envoy/upstream/thread_local_cluster.h"

#include "source/common/http/message_impl.h"
#include "source/common/router/config_impl.h"
#include "source/common/event/dispatcher_impl.h"
#include "source/extensions/filters/http/common/pass_through_filter_ex.h"

#include "filters/api/envoy/extensions/filters/http/anti_cc/v3/anti_cc.pb.h"
#include "filters/api/envoy/extensions/filters/http/anti_cc/v3/anti_cc_log.pb.h"

#include "impl/action.h"
#include "filters/source/extensions/filters/http/common/ratelimit/ratelimit_client.h"
#include "filters/source/extensions/filters/http/common/ip_whitelist/ip_whitelist.h"
#include "filters/source/extensions/filters/http/common/central_database/database.h"

#define FILTER_NAME "envoy.filters.http.anti-cc.1.0"
#define CODE_GET_HTML "1"
#define CODE_GET_CODE "2"
#define CODE_VERIFY_CODE "3"
#define VERIFY_RESULT_SUCCESS "1"
#define VERIFY_RESULT_FAILURE "2"
#define FIX_VERIFY "verify"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AntiCC {

namespace v3 = envoy::extensions::filters::http::anti_cc::v3;

// 全局配置
class AntiCCFilterGlobalConfig : public Router::RouteSpecificFilterConfig {
public:
  AntiCCFilterGlobalConfig(const v3::AntiCCGlobal& proto_config,
                           Server::Configuration::ServerFactoryContext& context);

public:
  Upstream::ThreadLocalCluster* getVerifyCluster() const {
    return cluster_manager_.getThreadLocalCluster(verification_server_name_);
  }
  Upstream::ThreadLocalCluster* getPolyCubeCluster() const {
    return cluster_manager_.getThreadLocalCluster(polycube_server_name_);
  }
  std::chrono::milliseconds httpRequestTimeout() const { return http_request_timeout_; }
  v3::Mode mode() const { return mode_; }
  v3::AutoLevel autoLevel() const { return auto_level_; }
  const Impl::ActionPtr& action() const { return action_; }
  const std::vector<Filters::Common::IpWhitelist::IpWhitelistPtr>& ipWhitelist() const {
    return ip_whitelist_;
  }

private:
  Upstream::ClusterManager& cluster_manager_;
  // 验证服务集群名
  const std::string verification_server_name_;
  // polycube集群名
  const std::string polycube_server_name_;
  // http请求超时时间
  const std::chrono::milliseconds http_request_timeout_;
  // 模式
  const v3::Mode mode_;
  // 自动模式等级
  const v3::AutoLevel auto_level_;
  // 防护动作
  const Impl::ActionPtr action_;
  // ip白名单
  const std::vector<Filters::Common::IpWhitelist::IpWhitelistPtr> ip_whitelist_;
};
using FilterGlobalConfigSharedPtr = std::shared_ptr<AntiCCFilterGlobalConfig>;

// Filter类
class AntiCCFilter : public Http::PassThroughFilterEx,
                     public Http::AsyncClient::Callbacks,
                     public Filters::Common::RatelimitClient::LimitRequestCallbacks,
                     public Filters::Common::RatelimitClient::CleanRequestCallbacks,
                     public Logger::Loggable<Logger::Id::filter> {
public:
  AntiCCFilter(Server::Configuration::FactoryContext& context, FilterGlobalConfigSharedPtr config,
               Filters::Common::CentralDatabase::DatabasePtr&& blacklist_kv_db,
               Filters::Common::CentralDatabase::DatabasePtr&& verify_status_kv_db,
               Filters::Common::RatelimitClient::ClientPtr&& ratelimit_client);

  // Http::PassThroughFilterEx
  void onDestroy() override;
  void onStreamComplete() override;
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance& data, bool end_stream) override;

  // Http::AsyncClient::Callbacks.
  void onSuccess(const Http::AsyncClient::Request&, Http::ResponseMessagePtr&&) override;
  void onFailure(const Http::AsyncClient::Request&, Http::AsyncClient::FailureReason) override;
  void onBeforeFinalizeUpstreamSpan(Tracing::Span&, const Http::ResponseHeaderMap*) override {}

  // Filters::Common::RatelimitClient::LimitRequestCallbacks
  void complete(Filters::Common::RatelimitClient::LimitStatus status,
                Filters::Common::RatelimitClient::LimitGrpcResponsePtr&& response) override;
  // Filters::Common::RatelimitClient::CleanRequestCallbacks
  void complete(Filters::Common::RatelimitClient::CleanStatus status) override;

private:
  enum class RequestMode { GetHtml, GetCode, Verify, BlockIp };
  enum class State { NotStarted, CallingLimit, CallingHttp, CallingKv, Complete, Responded };
  inline const Network::Address::InstanceConstSharedPtr getDownstreamAddress() const;
  const std::string& getLocalAddress() const;
  Http::AsyncClient::Request* sendAsyncHttpRequest(RequestMode mode, const std::string& body);
  void sendAsyncGrpcRateLimitRequest(bool is_verify);
  void sendAsyncGrpcCleanQuotaRequest(const std::string& src_ip);
  bool requestBodyAddJsonPair(std::string& request_body, const std::string& key,
                              const std::string& value);
  template <typename ValueType>
  bool requestBodyAddJsonPair(const std::string& old_body, std::string& new_body,
                              const std::string& key, const ValueType& value);
  bool requestBodyAddSrcIp(std::string& request_body);
  bool requestBodyAddSrcIp(const std::string& old_body, std::string& new_body);
  bool requestBodyAddPath(std::string& request_body);
  bool requestBodyAddBlacklist(std::string& request_body);
  void getFromRemoteCache(const std::string& key);
  void insertToRemoteCache(const std::string& key, const std::string&, uint32_t ttl);
  void getFromRemoteCacheCallback(int result, const std::string& key, const void* value);
  void findVerifyStatus(const std::string& src_ip, Filters::Common::CentralDatabase::DatabasePtr& kv_db,
            std::function<void(int)> cb);
  void insertVerifyStatus(const std::string& src_ip, Filters::Common::CentralDatabase::DatabasePtr& kv_db);
  void deleteVerifyStatus(const std::string& src_ip, Filters::Common::CentralDatabase::DatabasePtr& kv_db);
  void setLog(v3::Act act, uint32_t duration = 0);

private:
  FilterGlobalConfigSharedPtr filter_hcm_config_;
  Filters::Common::RatelimitClient::ClientPtr ratelimit_client_;
  Filters::Common::CentralDatabase::DatabasePtr blacklist_kv_db_;
  Filters::Common::CentralDatabase::DatabasePtr verify_status_kv_db_;
  State state_{State::NotStarted};
  std::vector<std::shared_ptr<Filters::Common::RatelimitClient::Impl::RateLimitPolicy>>
      ratelimit_policys_{};
  bool dryrun_{};
  bool process_verify_quota_{};
  bool has_verification_result_header_{};
  bool is_add_blacklist_request_{};
  bool is_log_request_{};
  uint32_t hit_duration_{};
  Http::RequestHeaderMap* request_headers_{nullptr};
  Http::AsyncClient::Request* request_{nullptr};
  Filters::Common::RatelimitClient::LimitGrpcResponsePtr ratelimit_grpc_response_;
  std::string src_ip_and_host_;
  std::function<void(int)> get_verify_status_cb_;
  static const std::string filter_name_;
  static const Http::LowerCaseString header_ratelimit_duration_;
  static const Http::LowerCaseString header_ratelimit_remaining_;
  static const Http::LowerCaseString header_ratelimit_drop_;
  static const Http::LowerCaseString header_verification_result_;
  static const std::string header_path_get_html_;
  static const std::string header_path_get_code_;
  static const std::string header_path_code_verify_;
  static const std::string header_path_add_blacklist_;
  static const std::string src_ip_str_;
  static const std::string path_str_;
  static const std::string ip_str_;
  static const std::string ttl_str_;
  static const std::string type_str_;
  static const std::string source_blacklist_str_;
  static const std::string src_and_dest_blacklist_str_;
  static const char* data_str_;
  static const char* value_str_;
  static const char* mode_str_;
  static const char* result_str_;
  v3::AntiCCLog log_;
  bool is_report_access_log_{false};
};
} // namespace AntiCC
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy