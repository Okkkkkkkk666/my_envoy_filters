#include "filter.h"

#include <cmath>

#include "impl/cookie.h"
#include "impl/http_header.h"
#include "impl/source_ip.h"
#include "impl/ssl_session.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {
const std::string Filter::filter_name_(FILTER_NAME);
Filters::Common::CentralDatabase::DatabasePtr Filter::null_kv_db_;

FilterGlobalConfig::FilterGlobalConfig(const v3::StrongStatefulSessionGlobal& proto_config,
                                       Server::Configuration::FactoryContext& context)
    : stateful_session_(createStatefulSession(proto_config)) {
  if (proto_config.has_grpc_service()) {
    const std::chrono::milliseconds timeout = std::chrono::milliseconds(
        PROTOBUF_GET_MS_OR_DEFAULT(proto_config.grpc_service(), timeout, 20));
    kv_db_ = std::make_unique<Filters::Common::CentralDatabase::Database>(
        context, proto_config.grpc_service(), timeout, FILTER_NAME);
  }
  if (kv_db_) {
    initRemoteCache(kv_db_, [&]() { remote_cache_init_ = true; });
  }
}

FilterGlobalConfig::~FilterGlobalConfig() {
  if (kv_db_) {
    kv_db_->cancel();
  }
}

void FilterGlobalConfig::initRemoteCache(Filters::Common::CentralDatabase::DatabasePtr& kv_db,
                                         std::function<void()> cb) {
  if (kv_db) {
    kv_db->cleanAsync([cb](bool) {
      if (cb) {
        cb();
      }
    });
  }
}

std::unique_ptr<Impl::StatefulSession>
FilterGlobalConfig::createStatefulSession(const v3::StrongStatefulSessionGlobal& proto_config) {
  std::unique_ptr<Impl::StatefulSession> stateful_session;
  if (proto_config.has_src_ip()) {
    stateful_session = std::make_unique<Impl::SourceIp>(proto_config);
  } else if (proto_config.has_ssl_session()) {
    stateful_session = std::make_unique<Impl::SslSession>(proto_config);
  } else if (proto_config.has_header()) {
    stateful_session = std::make_unique<Impl::HttpHeader>(proto_config);
  } else if (proto_config.has_cookie()) {
    stateful_session = std::make_unique<Impl::Cookie>(proto_config);
  }

  return stateful_session;
}

// 路由级别配置
FilterRouteConfig::FilterRouteConfig(const v3::StrongStatefulSessionPerRoute& proto_config)
    : stateful_session_(createStatefulSession(proto_config)){}

std::unique_ptr<Impl::StatefulSession>
FilterRouteConfig::createStatefulSession(const v3::StrongStatefulSessionPerRoute& proto_config) {
  std::unique_ptr<Impl::StatefulSession> stateful_session;
  if (proto_config.has_src_ip()) {
    stateful_session = std::make_unique<Impl::SourceIp>(proto_config);
  } else if (proto_config.has_ssl_session()) {
    stateful_session = std::make_unique<Impl::SslSession>(proto_config);
  } else if (proto_config.has_header()) {
    stateful_session = std::make_unique<Impl::HttpHeader>(proto_config);
  } else if (proto_config.has_cookie()) {
    stateful_session = std::make_unique<Impl::Cookie>(proto_config);
  }

  return stateful_session;
}

FilterRouteConfig::~FilterRouteConfig() {}

// 获取虚拟服务配置
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

// 获取路由配置
inline const FilterRouteConfig*
Filter::getRouteConfig(const FilterRouteConfig* filter_vh_config) const {
  const FilterRouteConfig* filter_route_config = nullptr;
  auto route = decoder_callbacks_->route();
  if (route) {
    filter_route_config =
        dynamic_cast<const FilterRouteConfig*>(route->mostSpecificPerFilterConfig(filter_name_));
  }

  // 路由级别的配置不存在时，mostSpecificPerFilterConfig返回的是VH级别的配置
  return filter_route_config == filter_vh_config ? nullptr : filter_route_config;
}

inline const Envoy::Router::VirtualHostImpl* Filter::getVirtualHost() const {
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

const std::unique_ptr<Impl::StatefulSession>& Filter::getStatefulSession() const {
  auto& statefulSession = filter_hcm_config_->statefulSession();
  if (statefulSession == nullptr) {
    //  无全局配置，执行虚拟服务、路由配置
    const Envoy::Router::VirtualHostImpl* vh = getVirtualHost();
    const FilterRouteConfig* filter_vh_config = getVirtualHostConfig(vh);

    // 获取路由配置
    const FilterRouteConfig* filter_route_config = getRouteConfig(filter_vh_config);
    if (filter_route_config != nullptr) {
      ENVOY_LOG(trace, "route config");
      return filter_route_config->statefulSession();
    } else if (filter_vh_config != nullptr) {
      ENVOY_LOG(trace, "vh config");
      return filter_vh_config->statefulSession();
    }
  }
  return statefulSession;
}

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers,
                                                bool /*end_stream*/) {
  ENVOY_LOG(trace, "decodeHeaders is runing...");

  // 保证远程缓存初始化完毕后才启用插件功能
  if (filter_hcm_config_->isRemoteCacheInit() == false) {
    ENVOY_LOG(trace, "remote not init");
    return Http::FilterHeadersStatus::Continue;
  }

  request_headers_ = &headers;

  auto& statefulSession = getStatefulSession();

  if (statefulSession != nullptr) {
    bool need_sync = statefulSession->upstreamAddress(
        headers, decoder_callbacks_->streamInfo(), upstream_override_host_, kv_db_, [&]() {
          statefulSession->upstreamAddress(headers, decoder_callbacks_->streamInfo(),
                                           upstream_override_host_, null_kv_db_, nullptr);

          if (!upstream_override_host_.empty()) {
            ENVOY_LOG(trace, "setUpstreamOverrideHost:{}", upstream_override_host_);
            statefulSession->setUpstreamOverrideHost(decoder_callbacks_, upstream_override_host_);
          }

          decoder_callbacks_->continueDecoding();
        });

    if (need_sync) {
      ENVOY_LOG(trace, "waiting for grpc");
      return Http::FilterHeadersStatus::StopAllIterationAndWatermark;
    }

    if (!upstream_override_host_.empty()) {
      ENVOY_LOG(trace, "setUpstreamOverrideHost:{}", upstream_override_host_);
      statefulSession->setUpstreamOverrideHost(decoder_callbacks_, upstream_override_host_);
    }
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap& headers, bool) {
  ENVOY_LOG(trace, "encodeHeaders is runing...");

  // 保证远程缓存初始化完毕后才启用插件功能
  if (filter_hcm_config_->isRemoteCacheInit() == false) {
    return Http::FilterHeadersStatus::Continue;
  }

  auto& statefulSession = getStatefulSession();

  if (statefulSession != nullptr) {
    if (auto upstream_info = encoder_callbacks_->streamInfo().upstreamInfo();
        upstream_info != nullptr) {
      auto host = upstream_info->upstreamHost();
      if (host != nullptr) {
        bool need_sync = statefulSession->update(*host, *request_headers_, headers,
                                                 encoder_callbacks_->streamInfo(), kv_db_,
                                                 [&]() { encoder_callbacks_->continueEncoding(); });

        if (need_sync) {
          ENVOY_LOG(trace, "waiting for grpc");
          return Http::FilterHeadersStatus::StopAllIterationAndWatermark;
        }
      }
    }
  }

  return Http::FilterHeadersStatus::Continue;
}

void Filter::onDestroy() {
  if (kv_db_) {
    kv_db_->cancel();
  }
}

} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy