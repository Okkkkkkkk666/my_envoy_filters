#pragma once

#include <format>
#include "envoy/srhino_plugin_framework/v1_0_x/libs/net/http_call.h"
#include "envoy/server/factory_context.h"
#include "source/common/common/assert.h"
#include "source/common/common/thread.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace Net {
/**
 * 注意，本类实现不是线程安全的。多线程环境下需要使用不同的实例。
 */
class HttpCallImpl : public HttpCall, public Envoy::Http::AsyncClient::Callbacks {
  friend class HttpCallTest;
public:
  HttpCallImpl(Envoy::Server::Configuration::FactoryContext& factory_context,
               std::string&& cluster_name, std::chrono::milliseconds timeout)
      : factory_context_(factory_context), cluster_name_(std::move(cluster_name)),
        timeout_(validateTimeout(timeout) ? timeout : std::chrono::milliseconds(1000)) {}

  ~HttpCallImpl() { cancel(); }

public:
  bool get(const std::string_view& path, const HeaderMap& headers, const DataSlices& body,
           HttpCall::Callback cb) override;
  bool get(const std::string_view& path, const HeaderMap& headers, Callback cb) override;
  bool get(const std::string_view& path,
           const std::initializer_list<std::pair<std::string_view, std::string_view>>& headers,
           const std::string_view& body, HttpCall::Callback cb) override;
  bool post(const std::string_view& path, const HeaderMap& headers, const DataSlices& body,
            HttpCall::Callback cb) override;
  bool post(const std::string_view& path, const HeaderMap& headers, Callback cb) override;
  bool post(const std::string_view& path,
            const std::initializer_list<std::pair<std::string_view, std::string_view>>& headers,
            const std::string_view& body, HttpCall::Callback cb) override;
  void cancel() override;

public:
  std::chrono::milliseconds timeout() const { return timeout_; }
  void timeout(std::chrono::milliseconds timeout) { timeout_ = timeout; }
  static bool validateTimeout(const std::chrono::milliseconds& timeout);

  // Http::AsyncClient::Callbacks.
private:
  void onSuccess(const Envoy::Http::AsyncClient::Request&,
                 Envoy::Http::ResponseMessagePtr&&) override;
  void onFailure(const Envoy::Http::AsyncClient::Request&,
                 Envoy::Http::AsyncClient::FailureReason) override;
  void onBeforeFinalizeUpstreamSpan(Envoy::Tracing::Span&,
                                    const Envoy::Http::ResponseHeaderMap*) override {}

private:
  bool send(const absl::string_view& method, const std::string_view& path, const HeaderMap& headers,
            const DataSlices& body);
  static std::string makeHost(const std::string_view& address, uint16_t port);

private:
  Envoy::Server::Configuration::FactoryContext& factory_context_;
  std::string cluster_name_;
  std::chrono::milliseconds timeout_{};
  Envoy::Http::AsyncClient::Request* http_request_{nullptr};
  HttpCall::Callback callback_;
};
} // namespace Net
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework