#pragma once

#include <unordered_map>

#include "envoy/srhino_plugin_framework/v1_2_x/context.h"
#include "source/common/srhino_plugin_framework/v1_2_x/header_map_impl.h"
#include "source/common/srhino_plugin_framework/v1_2_x/data_slices_impl.h"
#include "source/common/srhino_plugin_framework/v1_2_x/virtual_host_info_impl.h"
#include "source/common/srhino_plugin_framework/v1_2_x/route_info_impl.h"
#include "source/common/srhino_plugin_framework/v1_2_x/connection_info_impl.h"
#include "source/common/srhino_plugin_framework/v1_2_x/thread_info_impl.h"

#define BREAK_BODY_DEFAULT true

namespace SrhinoPluginFramework {
namespace v1_2_x {
class ContextImpl : public virtual Context {
public:
  ContextImpl(const std::string& file_name,
              Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks,
              Envoy::Http::StreamEncoderFilterCallbacks* encoder_callbacks,
              Envoy::Server::Configuration::FactoryContext& factory_context)
      : decoder_callbacks_(decoder_callbacks), encoder_callbacks_(encoder_callbacks),
        stream_callbacks_(
            decoder_callbacks != nullptr
                ? static_cast<Envoy::Http::StreamFilterCallbacks*>(decoder_callbacks)
                : static_cast<Envoy::Http::StreamFilterCallbacks*>(encoder_callbacks)),
        factory_context_(factory_context),
        virtual_host_info_(file_name, stream_callbacks_, factory_context),
        route_info_(file_name, stream_callbacks_), connection_info_(stream_callbacks_),
        thread_info_(stream_callbacks_), buffer_pool_(decoder_callbacks, encoder_callbacks) {
    ASSERT((decoder_callbacks && !encoder_callbacks) || (encoder_callbacks && !decoder_callbacks));
    ASSERT(stream_callbacks_);
  }

  // override
public:
  const VirtualHostInfo& virtualHostInfo() const override { return virtual_host_info_; }
  const RouteInfo& routeInfo() const override { return route_info_; }
  const ConnectionInfo& connectionInfo() const override { return connection_info_; }
  const ThreadInfo& threadInfo() const override { return thread_info_; }
  void setDirectResponse(Utility::HttpCode code, std::unique_ptr<HeaderMap>&& headers,
                         std::string&& body) override;
  void setDirectResponse(Utility::HttpCode code, std::unique_ptr<HeaderMap>&& headers,
                         const std::string& body) override;
  void directResponseInternal(Utility::HttpCode code, std::unique_ptr<HeaderMap>&& headers,
                              absl::string_view body);
  std::unordered_map<std::string, std::string>& sharedData() override;
  TimerSharedPtr createTimer(std::chrono::milliseconds interval,
                             std::function<bool(Timer& timer)> func,
                             bool run_in_main_thread = true) override;
  void overrideDestHost(const std::string_view& host) override;
  void setWaitForBodyBufferLimit(uint32_t size) override;
  uint32_t waitForBodyBufferLimit() const override;
  std::unique_ptr<HeaderMap> createHeaderMap() const;
  std::unique_ptr<HeaderMap> createHeaderMap(
      const std::initializer_list<std::pair<std::string_view, std::string_view>>& headers) const;

public:
  struct DirectResponse {
    Utility::HttpCode code_{0};
    std::unique_ptr<HeaderMap> headers_;
    std::string body_str_;
    absl::string_view body_;
  };

  const DirectResponse& directResponse() const { return direct_response_; }
  Envoy::Server::Configuration::FactoryContext& factoryContext() const { return factory_context_; }
  Envoy::Http::StreamDecoderFilterCallbacks* streamDecoderFilterCallbacks() const {
    return decoder_callbacks_;
  }
  Envoy::Http::StreamEncoderFilterCallbacks* streamEncoderFilterCallbacks() const {
    return encoder_callbacks_;
  }

public:

  /**
   * 为了给每个插件提供独立的缓冲区大小设置，该类抽象出缓冲池的概念。
   * 
   * Envoy原生的buffer机制，每个stream是独立的缓冲区，但该stream的插件链中的所有插件则共享此缓冲区。
   * 这意味着当某个插件调用Envoy::Http::StreamDecoderFilterCallbacks::setDecoderBufferLimit、
   * Envoy::Http::StreamEncoderFilterCallbacks::setEncoderBufferLimit进行缓冲区设置时将影响插件链中
   * 的所有插件。
   * 
   * 缓冲池利用原生的buffer机制，通过封装setDecoderBufferLimit、setEncoderBufferLimit等方法，遵循缓冲
   * 池只能向上增长的原则来实现给每个插件提供独立的缓冲区大小设置这一功能。核心思想是缓冲池提供一个足够大的
   * 缓冲区，其大小可以满足插件链中的所有插件，每个插件则使用缓冲区中的一部分。示例：
   * 
   *                      10M           15M                                                50M
   * +---------------------+-------------+--------------------------------------------------+
   * ╰----------┬----------╯      
   *     A 插件设置10M缓冲区大小
   * ╰-----------------┬-----------------╯    
   *           B 插件设置15M缓冲区大小
   * ╰------------------------------------------┬-------------------------------------------╯
   *                                    C 插件设置50M缓冲区大小   
   *    
   * 从示例可知，此时缓冲池设置的缓冲区大小实际是插件链中插件设置的最大值。
   * 具体实现也挺简单，即当调用setDecoderBufferLimit、setEncoderBufferLimit时，不允许设置比当前缓冲区小的
   * 值，这样可以保证不影响其他的插件。
   */
  class StreamBufferPool {
  public:
    StreamBufferPool(Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks,
                     Envoy::Http::StreamEncoderFilterCallbacks* encoder_callbacks)
        : decoder_callbacks_(decoder_callbacks), encoder_callbacks_(encoder_callbacks) {
      ASSERT((decoder_callbacks && !encoder_callbacks) ||
             (encoder_callbacks && !decoder_callbacks));
    }

  public:
    void limit(uint32_t size);
    uint32_t limit() const;

  private:
    void grow(uint32_t size);

  private:
    Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks_;
    Envoy::Http::StreamEncoderFilterCallbacks* encoder_callbacks_;
    std::optional<uint32_t> limit_;
  };

protected:
  Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks_;
  Envoy::Http::StreamEncoderFilterCallbacks* encoder_callbacks_;
  Envoy::Http::StreamFilterCallbacks* stream_callbacks_;
  Envoy::Server::Configuration::FactoryContext& factory_context_;
  DirectResponse direct_response_;

private:
  VirtualHostInfoImpl virtual_host_info_;
  RouteInfoImpl route_info_;
  ConnectionInfoImpl connection_info_;
  ThreadInfoImpl thread_info_;
  StreamBufferPool buffer_pool_;
};

class HeaderContextImpl : public HeaderContext, public ContextImpl {
public:
  HeaderContextImpl(const std::string& file_name,
                    Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks,
                    Envoy::Http::StreamEncoderFilterCallbacks* encoder_callbacks,
                    Envoy::Server::Configuration::FactoryContext& factory_context,
                    Envoy::Http::HeaderMap& headers, bool end_stream)
      : ContextImpl(file_name, decoder_callbacks, encoder_callbacks, factory_context),
        end_stream_(end_stream), headers_(&headers) {}

  // override
public:
  void setDirectResponse(Utility::HttpCode code, std::unique_ptr<HeaderMap>&& headers,
                         std::string&& body) override;
  void setDirectResponse(Utility::HttpCode code, std::unique_ptr<HeaderMap>&& headers,
                         const std::string& body) override;
  HeaderMap& headers() override { return headers_; };
  const HeaderMap& headers() const override { return headers_; };
  void setBreakBody(bool is_break_body) override { is_break_body_ = is_break_body; }
  bool hasBody() const override { return !end_stream_; }
  void continued() const override;

public:
  bool isBreakBody() const { return is_break_body_; }
  void setPause(bool pause) { is_pause_ = pause; }

private:
  bool end_stream_;
  HeaderMapImpl headers_;
  bool is_break_body_{BREAK_BODY_DEFAULT};
  bool is_pause_{false};
};

class BodyContextImpl : public BodyContext, public ContextImpl {
public:
  BodyContextImpl(const std::string& file_name,
                  Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks,
                  Envoy::Http::StreamEncoderFilterCallbacks* encoder_callbacks,
                  Envoy::Server::Configuration::FactoryContext& factory_context, bool end_stream)
      : ContextImpl(file_name, decoder_callbacks, encoder_callbacks, factory_context),
        end_stream_(end_stream) {}
  // override
public:
  void setDirectResponse(Utility::HttpCode code, std::unique_ptr<HeaderMap>&& headers,
                         std::string&& body) override;
  void setDirectResponse(Utility::HttpCode code, std::unique_ptr<HeaderMap>&& headers,
                         const std::string& body) override;
  DataSlices& data() override { return data_; }
  bool hasMoreBody() const override { return !end_stream_; }
  void continued() const override;

public:
  void setData(Envoy::Buffer::Instance& data) { data_.setData(data); }
  void setPause(bool pause) { is_pause_ = pause; }
  void setEndStream(bool end_stream) { end_stream_ = end_stream; }

private:
  bool end_stream_;
  DataSlicesImpl data_;
  bool is_pause_{false};
};

class TrailersContextImpl : public TrailerContext, public ContextImpl {
public:
  TrailersContextImpl(const std::string& file_name,
                      Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks,
                      Envoy::Http::StreamEncoderFilterCallbacks* encoder_callbacks,
                      Envoy::Server::Configuration::FactoryContext& factory_context,
                      Envoy::Http::HeaderMap& headers)
      : ContextImpl(file_name, decoder_callbacks, encoder_callbacks, factory_context),
        headers_(&headers) {}

  // override
public:
  HeaderMap& headers() override { return headers_; };
  const HeaderMap& headers() const override { return headers_; };

private:
  HeaderMapImpl headers_;
};
} // namespace v1_2_x
} // namespace SrhinoPluginFramework