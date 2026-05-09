#pragma once

#include "envoy/runtime/runtime.h"
#include "envoy/upstream/cluster_manager.h"
#include "envoy/upstream/thread_local_cluster.h"

#include "source/common/router/config_impl.h"
#include "source/extensions/filters/http/common/pass_through_filter_ex.h"

#include "envoy/type/v3/percent.pb.h"

#include "filters/api/envoy/extensions/filters/http/super_glue/v3/super_glue_log.pb.h"
#include "filters/api/envoy/extensions/filters/http/super_glue/v3/super_glue_client.pb.h"

#include "rule.h"
#include "common.h"

#define FILTER_NAME "envoy.filters.http.super-glue-client.1.0"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SuperGlueFilter {

namespace v3 = envoy::extensions::filters::http::super_glue::v3;

class Cluster {
public:
  Cluster(const v3::Cluster& cluster);
  const std::string& clusterName() const { return cluster_; }
  bool unhealthyBypass() const { return unhealthy_bypass_; }
  bool timeoutBypass() const { return timeout_bypass_; }
  const std::chrono::milliseconds& requestTimeout() const { return request_timeout_; }

private:
  const std::string cluster_;
  const bool unhealthy_bypass_;
  const bool timeout_bypass_;
  const std::chrono::milliseconds request_timeout_;
};
using ClusterPtr = std::unique_ptr<Cluster>;

// 全局配置
class ClientFilterGlobalConfig : public Router::RouteSpecificFilterConfig {
public:
  ClientFilterGlobalConfig(const v3::SuperGlueClientGlobal& proto_config,
                           Server::Configuration::FactoryContext& context);
};

using ClientFilterGlobalConfigSharedPtr = std::shared_ptr<ClientFilterGlobalConfig>;

// VH配置
class ClientFilterRouteConfig : public Router::RouteSpecificFilterConfig {
public:
  ClientFilterRouteConfig(const v3::SuperGlueClientRoute& proto_config,
                          Server::Configuration::ServerFactoryContext& context);

public:
  Upstream::ThreadLocalCluster* getThreadLocalCluster(const std::string& cluster) const {
    return cluster_manager_.getThreadLocalCluster(cluster);
  }
  bool enable() const { return enable_; }
  const std::vector<ClusterPtr>& clusterList() const { return cluster_list_; }
  const std::vector<RulePtr>& rules() const { return rules_; }
  bool getFeatureEnabled() const {
    return runtime_.snapshot().featureEnabled(percent_key_, *percent_age_);
  }

private:
  Runtime::Loader& runtime_;
  Upstream::ClusterManager& cluster_manager_;
  const bool enable_;
  const std::vector<ClusterPtr> cluster_list_;
  const std::vector<RulePtr> rules_;
  const std::shared_ptr<envoy::type::v3::FractionalPercent> percent_age_;
  static const std::string percent_key_;
};

using ClientFilterRouteConfigSharedPtr = std::shared_ptr<ClientFilterRouteConfig>;

class ClientFilter : public Http::PassThroughFilterEx, public Http::AsyncClient::Callbacks {
public:
  ClientFilter(ClientFilterGlobalConfigSharedPtr config,
               const Server::Configuration::ServerFactoryContext& context)
      : Http::PassThroughFilterEx(context), filter_hcm_config_(config) {}

  // Http::PassThroughFilterEx
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance& data, bool end_stream) override;
  void onStreamComplete() override;
  void onDestroy() override;

  // Http::AsyncClient::Callbacks.
  void onSuccess(const Http::AsyncClient::Request&, Http::ResponseMessagePtr&&) override;
  void onFailure(const Http::AsyncClient::Request&, Http::AsyncClient::FailureReason) override;
  void onBeforeFinalizeUpstreamSpan(Tracing::Span&, const Http::ResponseHeaderMap*) override {}

private:
  bool handleRequest();
  inline const Envoy::Router::VirtualHostImpl* getVirtualHost() const;
  inline const ClientFilterRouteConfig*
  getVirtualHostConfig(const Envoy::Router::VirtualHostImpl* vh) const;
  inline const ClientFilterRouteConfig* getRouteConfig() const;
  inline const Network::Address::InstanceConstSharedPtr getDownstreamAddress() const;
  inline std::string getUsername() const;
  inline const std::string& getUpstreamName() const;

  // accessLog
  inline void accessLogAddCluster(v3::ClusterLog_Act act);
  inline void accessLogSetEndpointStatus(v3::ClusterLog* cluster_log);

  Http::AsyncClient::Request* sendFirstAsyncHttpRequest();
  Http::AsyncClient::Request* sendNextAsyncHttpRequest();
  Http::AsyncClient::Request* sendAsyncHttpRequest();

private:
  ClientFilterGlobalConfigSharedPtr filter_hcm_config_;
  const ClientFilterRouteConfig* filter_vh_config_;
  Http::RequestHeaderMap* request_headers_{nullptr};
  Http::AsyncClient::Request* request_{nullptr};
  static const std::string filter_name_;
  static const std::string unhealthy_response_body_;
  static const std::string timeout_response_body_;
  static const std::string srhino_domain_metadata_;
  static const std::string srhino_key_user_name_;
  std::vector<ClusterPtr>::const_iterator current_cluster_;
  std::string upstream_address_;
  bool is_match_{false};
  bool end_stream_{false};
  bool is_buffer_full_{false};
  v3::SuperGlueLog log_;
  std::string last_body_block_{};
};
} // namespace SuperGlueFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
