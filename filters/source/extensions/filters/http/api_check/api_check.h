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

#include "filters/source/extensions/filters/http/api_check/impl/waf_check_mode.h"
#include "filters/source/extensions/filters/http/waf/waf.h"
#include "filters/api/envoy/extensions/filters/http/api_check/v3/api_check.pb.h"
#include "filters/api/envoy/extensions/filters/http/api_check/v3/api_check_log.pb.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ApiCheckFilter {

constexpr char filter_name[] = "envoy.filters.http.api-check.1.0";

namespace v3 = envoy::extensions::filters::http::api_check::v3;
using WafFilterGlobalConfigSharedPtr = std::shared_ptr<WafFilter::FilterGlobalConfig>;
using WafFilterRouteConfigSharedPtr = std::shared_ptr<WafFilter::FilterRouteConfig>;
using WafFilterGlobalSharedPtr = std::shared_ptr<WafFilter::Filter>;

class FilterRouteConfig : public Router::RouteSpecificFilterConfig {
public:
  FilterRouteConfig(const v3::ApiCheckRoute& proto_config);
  bool disable_request() const { return disable_request_; }
  bool disable_response() const { return disable_response_; }
  WafFilterRouteConfigSharedPtr waf_route_config() const { return waf_route_config_; }
  void setWafRouteProtoConfig(Impl::waf_v3::WafRoute& waf_route_proto_config);
  const Impl::waf_v3::WafRoute& waf_route_proto_config() const { return waf_route_proto_config_; }

private:
  const bool disable_request_;
  const bool disable_response_;
  WafFilterRouteConfigSharedPtr waf_route_config_;
  Impl::waf_v3::WafRoute waf_route_proto_config_;
};

class FilterGlobalConfig : public Logger::Loggable<Logger::Id::filter> {
public:
  FilterGlobalConfig(const v3::ApiCheckGlobal& proto_config, const std::string& stats_prefix,
                     Server::Configuration::FactoryContext&);
  ~FilterGlobalConfig();
  WafFilterGlobalConfigSharedPtr waf_global_config() const { return waf_global_config_; }
  void setWafGlobalProtoConfig(Impl::waf_v3::WafGlobal& waf_global_proto_config_);
  const Impl::WafCheckModeConfig& getWafCheckModeConfig() const { return waf_check_mode_config_; }

private:
  WafFilterGlobalConfigSharedPtr waf_global_config_;
  Impl::waf_v3::WafGlobal waf_global_proto_config_;
  Impl::WafCheckModeConfig waf_check_mode_config_;
};
using FilterGlobalConfigSharedPtr = std::shared_ptr<FilterGlobalConfig>;

// 继承waf filter，获取自身的虚拟服务级别配置
class ApiCheckWaFilter : public WafFilter::Filter {
public:
  ApiCheckWaFilter(WafFilterGlobalConfigSharedPtr config,
                   Server::Configuration::ServerFactoryContext& context)
      : WafFilter::Filter(config, context) {}
  const WafFilter::FilterRouteConfig*
  getVirtualHostConfig(const Envoy::Router::VirtualHostImpl* vh) const override;
};

class Filter : public Http::PassThroughFilterEx, public Logger::Loggable<Logger::Id::filter> {
public:
  Filter(FilterGlobalConfigSharedPtr, Server::Configuration::ServerFactoryContext&);
  ~Filter() {}

  // Http::StreamFilterBase
  void onDestroy() override;
  void onStreamComplete() override;
  v3::ApiCheckLog& getLog() { return log_; }
  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance&, bool end_stream) override;

  // Http::StreamEncoderFilter
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool end_stream) override;
  Http::FilterDataStatus encodeData(Buffer::Instance&, bool end_stream) override;
  FilterGlobalConfigSharedPtr getConfig() { return config_; }

  // 设置内部waf请求流
  void setDecoderFilterCallbacks(Http::StreamDecoderFilterCallbacks& callbacks) override {
    PassThroughFilterEx::setDecoderFilterCallbacks(callbacks);
    waf_filter_->setDecoderFilterCallbacks(*(this->decoder_callbacks_));
  }
  // 设置内部waf响应流
  void setEncoderFilterCallbacks(Http::StreamEncoderFilterCallbacks& callbacks) override {
    PassThroughFilterEx::setEncoderFilterCallbacks(callbacks);
    waf_filter_->setEncoderFilterCallbacks(*(this->encoder_callbacks_));
  }

private:
  v3::ApiCheckLog log_;
  const FilterGlobalConfigSharedPtr config_;
  WafFilterGlobalSharedPtr waf_filter_;
};

} // namespace ApiCheckFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
