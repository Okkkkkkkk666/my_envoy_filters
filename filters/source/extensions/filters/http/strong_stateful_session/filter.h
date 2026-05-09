#pragma once

#include "filters/source/extensions/filters/http/common/central_database/database.h"
#include "impl/stateful_session.h"
#include "source/common/router/config_impl.h"
#include "source/extensions/filters/http/common/pass_through_filter_ex.h"

#include "filters/api/envoy/extensions/filters/http/strong_stateful_session/v3/strong_stateful_session.pb.h"

#define FILTER_NAME "envoy.filters.http.strong-stateful-session.1.0"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {

namespace v3 = envoy::extensions::filters::http::strong_stateful_session::v3;

// 全局配置
class FilterGlobalConfig {

public:
  FilterGlobalConfig(const v3::StrongStatefulSessionGlobal& proto_config,
                     Server::Configuration::FactoryContext& context);
  ~FilterGlobalConfig();

public:
  const std::unique_ptr<Impl::StatefulSession>& statefulSession() const {
    return stateful_session_;
  }
  bool isRemoteCacheInit() const { return remote_cache_init_; }

private:
  std::unique_ptr<Impl::StatefulSession>
  createStatefulSession(const v3::StrongStatefulSessionGlobal& proto_config);
  /**
   * 初始化远程缓存
   * 内部实现调用CentralDatabase::cleanAsync方法删除所有键值对。
   * 该方法被设计用来在“运行时配置类”构造时调用，以便在每次下发配置时清空远程缓存。
   * @param kv_db KV存储客户端
   * @param cb 初始化缓存完毕后的回调
   */
  void initRemoteCache(Filters::Common::CentralDatabase::DatabasePtr& kv_db,
    std::function<void()> cb);

private:
  std::unique_ptr<Impl::StatefulSession> stateful_session_;
  bool remote_cache_init_{false};
  Filters::Common::CentralDatabase::DatabasePtr kv_db_;
};

using FilterGlobalConfigSharedPtr = std::shared_ptr<FilterGlobalConfig>;

// VH和路由配置
class FilterRouteConfig : public Router::RouteSpecificFilterConfig,
                          public Logger::Loggable<Logger::Id::filter> {

public:
  FilterRouteConfig(const v3::StrongStatefulSessionPerRoute& proto_config);
  ~FilterRouteConfig();

public:
  const std::unique_ptr<Impl::StatefulSession>& statefulSession() const {
    return stateful_session_;
  }

private:
  std::unique_ptr<Impl::StatefulSession>
  createStatefulSession(const v3::StrongStatefulSessionPerRoute& proto_config);

private:
  std::unique_ptr<Impl::StatefulSession> stateful_session_;
};
using FilterRouteConfigSharedPtr = std::shared_ptr<FilterRouteConfig>;

class Filter : public Http::PassThroughFilterEx, public Logger::Loggable<Logger::Id::filter> {
public:
  Filter(FilterGlobalConfigSharedPtr config,
         const Server::Configuration::ServerFactoryContext& context,
         Filters::Common::CentralDatabase::DatabasePtr&& kv_db)
      : Http::PassThroughFilterEx(context), filter_hcm_config_(config), kv_db_(std::move(kv_db)) {}

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap& headers, bool) override;
  void onDestroy() override;

private:
  inline const FilterRouteConfig*
  getVirtualHostConfig(const Envoy::Router::VirtualHostImpl* vh) const;
  inline const FilterRouteConfig* getRouteConfig(const FilterRouteConfig* filter_vh_config) const;
  inline const Envoy::Router::VirtualHostImpl* getVirtualHost() const;
  const std::unique_ptr<Impl::StatefulSession>& getStatefulSession() const;

private:
  // 全局配置
  FilterGlobalConfigSharedPtr filter_hcm_config_;
  // 路由配置
  FilterRouteConfigSharedPtr filter_config_;
  static const std::string filter_name_;
  Http::RequestHeaderMap* request_headers_{nullptr};
  std::string upstream_override_host_;
  Filters::Common::CentralDatabase::DatabasePtr kv_db_;
  static Filters::Common::CentralDatabase::DatabasePtr null_kv_db_;
};
} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
