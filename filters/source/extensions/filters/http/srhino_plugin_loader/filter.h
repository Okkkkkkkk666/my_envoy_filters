#pragma once

#include <optional>

#include "filters/source/extensions/filters/http/common/lru_cache.hpp"
#include "multi_version_helper.h"
#include "source/common/srhino_plugin_framework/plugin_file.h"
#include "source/extensions/filters/http/common/pass_through_filter_ex.h"

#include "filters/api/envoy/extensions/filters/http/srhino_plugin_loader/v3/srhino_plugin_loader.pb.h"
#include "source/common/network/address_impl.h"

#define FILTER_NAME "envoy.filters.http.srhino-plugin-loader.1.0"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SrhinoPluginLoaderFilter {

namespace v3 = envoy::extensions::filters::http::srhino_plugin_loader::v3;

// 全局配置
class FilterGlobalConfig : public Logger::Loggable<Logger::Id::filter> {

public:
  FilterGlobalConfig(const v3::SrhinoPluginLoaderGlobal& proto_config,
                     Server::Configuration::FactoryContext& context);
  ~FilterGlobalConfig();

public:
  SrhinoPluginFramework::PluginFileSharedPtr pluginFile() const { return plugin_file_; }
  void* runtimeConfig() const { return runtime_config_; }
  const Network::Address::InstanceConstSharedPtr& predictUpstreamRemoteAddress() const {
    return predict_upstream_remote_address_;
  }
  void setPredictUpstreamRemoteAddress(
      const Network::Address::InstanceConstSharedPtr& upstream_remote_address) {
    predict_upstream_remote_address_ = upstream_remote_address;
  }
  const std::string& filterInstanceName() const { return composite_filter_name_; }

private:
  void queryPluginFile(const std::string& file_name);
  void onBind(SrhinoPluginFramework::PluginFileSharedPtr plugin_file);
  void onUnbind();
  std::string makeBindedName();

private:
  const google::protobuf::Struct proto_config_;
  Envoy::Server::Configuration::FactoryContext& factory_context_;
  Envoy::Event::TimerPtr timer_;
  SrhinoPluginFramework::PluginFileSharedPtr plugin_file_;
  void* runtime_config_{nullptr};
  std::string composite_filter_name_;
  Envoy::Server::ConfigTracker::EntryOwnerPtr config_tracker_entry_;
  std::string binded_name_;
  static Filters::Common::LruCache<std::string, uint64_t> filter_epoch_;
  Network::Address::InstanceConstSharedPtr predict_upstream_remote_address_{};
};

using FilterGlobalConfigSharedPtr = std::shared_ptr<FilterGlobalConfig>;

// 路由配置
class FilterRouteConfig : public Envoy::Router::RouteSpecificFilterConfig,
                          public Envoy::Router::SrhinoRouteSpecificFilterConfig,
                          public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  FilterRouteConfig(const v3::SrhinoPluginLoaderPerRoute& proto_config,
                     Envoy::Server::Configuration::ServerFactoryContext& context);
  ~FilterRouteConfig();

public:
  SrhinoPluginFramework::PluginFileSharedPtr pluginFile() const { return plugin_file_; }
  void* runtimeConfig() const override {
    std::lock_guard<std::mutex> lock(runtime_config_mtx_);
    return runtime_config_;
  }

private:
  void queryPluginFile(const std::string& file_name);
  void onBind(SrhinoPluginFramework::PluginFileSharedPtr plugin_file);
  void onUnbind();

private:
  const google::protobuf::Struct proto_config_;
  Envoy::Server::Configuration::ServerFactoryContext& factory_context_;
  Envoy::Event::TimerPtr timer_;
  SrhinoPluginFramework::PluginFileSharedPtr plugin_file_;
  void* runtime_config_{nullptr};
  mutable std::mutex runtime_config_mtx_;
};

class Filter : public Http::PassThroughFilterEx, public Logger::Loggable<Logger::Id::filter> {
public:
  Filter(FilterGlobalConfigSharedPtr config, Server::Configuration::FactoryContext& factory_context)
      : Http::PassThroughFilterEx(factory_context.getServerFactoryContext()),
        factory_context_(factory_context), filter_hcm_config_(config) {
    plugin_file_ = filter_hcm_config_->pluginFile();
    if (plugin_file_) {
      plugin_instance_ = plugin_file_->createInstance(config->runtimeConfig());
    }
  }

  ~Filter() {
    if (plugin_instance_) {
      plugin_file_->destroyInstance(plugin_instance_);
      plugin_instance_ = nullptr;
    }
  }

  // StreamDecoderFilter override
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance& data, bool end_stream) override;
  Http::FilterTrailersStatus decodeTrailers(Http::RequestTrailerMap& trailers) override;

  // StreamEncoderFilter override
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap& headers, bool) override;
  Http::FilterDataStatus encodeData(Buffer::Instance& data, bool end_stream) override;
  Http::FilterTrailersStatus encodeTrailers(Http::ResponseTrailerMap& trailers) override;

  // StreamFilterBase override
  void onStreamComplete() override;

private:
  using FillBufferCB = std::function<Buffer::Instance*()>;

private:
  template <class StatusT, class ContextT, class StreamCallbackT>
  static void maybeSendLocalReply(StatusT status, const std::unique_ptr<ContextT>& context,
                                  StreamCallbackT* stream_callback) {
    if (status != StatusT::DirectResponse) {
      return;
    }

    const typename ContextT::DirectResponse& direct_response = context->directResponse();
    stream_callback->sendLocalReply(
        static_cast<Http::Code>(direct_response.code_), direct_response.body_,
        [&](Envoy::Http::HeaderMap& headers) {
          if (!direct_response.headers_) {
            return;
          }
          direct_response.headers_->traverse(
              [&](const std::string_view& key, const std::string_view& value) {
                Envoy::Http::LowerCaseString lower_key({key.data(), key.size()});
                headers.setCopy(lower_key, {value.data(), value.size()});
                return true;
              });
        },
        absl::nullopt, absl::string_view());
  }

  Http::FilterHeadersStatus onHeader(MultiVersionFilterStatus& filter_status,
                                     Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks,
                                     Envoy::Http::StreamEncoderFilterCallbacks* encoder_callbacks,
                                     Http::HeaderMap& headers, bool end_stream);
  Http::FilterDataStatus onData(MultiVersionFilterStatus& filter_status,
                                Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks,
                                Envoy::Http::StreamEncoderFilterCallbacks* encoder_callbacks,
                                bool& is_buffer_full, Buffer::Instance& data, bool end_stream);

  Http::FilterTrailersStatus onTrailer(MultiVersionFilterStatus& filter_status,
                                       Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks,
                                       Envoy::Http::StreamEncoderFilterCallbacks* encoder_callbacks,
                                       Http::HeaderMap& trailers);

  void adjustBufferLimit(Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks,
                         const Buffer::Instance& data);
  void adjustBufferLimit(Envoy::Http::StreamEncoderFilterCallbacks* encoder_callbacks,
                         const Buffer::Instance& data);
private:
  /**
   * 当插件在处理请求阶段发送本地回复时，stream_info.upstreamInfo()->upstreamHost()为nullptr，无法获取业务IP和端口。
   * 此函数可以预先获取请求将要发送的业务端，并设置进upstreamInfo中。
   */
  inline bool setPredictUpstreamRemoteAddress() const;

private:
  Server::Configuration::FactoryContext& factory_context_;
  FilterGlobalConfigSharedPtr filter_hcm_config_;
  static const std::string filter_name_;
  SrhinoPluginFramework::PluginFileSharedPtr plugin_file_;
  void* plugin_instance_{nullptr};
  MultiVersionFilterStatus decode_status_;
  MultiVersionFilterStatus encode_status_;
  bool is_buffer_full_{false};
  /**
   * 解决envoy回复426响应进encodeXXX问题
   */
  bool is_normal_stream_{false}; /* 标记正常流 */
};
} // namespace SrhinoPluginLoaderFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
