#pragma once

#include "envoy/server/factory_context.h"
#include "envoy/grpc/async_client_manager.h"

#include "source/common/grpc/common.h"
#include "source/common/grpc/typed_async_client.h"
#include "source/common/buffer/zero_copy_input_stream_impl.h"

#include "filters/api/envoy/extensions/filters/http/common/ratelimit/v3/ratelimit.pb.h"

#include "ratelimit_policy.h"
#include "filters/source/extensions/filters/http/common/ratelimit/ratelimit_client.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace RatelimitClient {
namespace Impl {

namespace v3 = envoy::extensions::filters::http::common::ratelimit::v3;

class GrpcClientImpl : public Filters::Common::RatelimitClient::Client,
                       public Grpc::RawAsyncRequestCallbacks,
                       public Logger::Loggable<Logger::Id::config> {
public:
  GrpcClientImpl(const Grpc::RawAsyncClientSharedPtr& async_client,
                 const absl::optional<std::chrono::milliseconds>& timeout);
  ~GrpcClientImpl() override;

  void createRequest(v3::RateLimitRequest& request,
                     const std::vector<std::shared_ptr<RateLimitPolicy>>& ratelimitpolicy) const;

  // Filters::Common::RatelimitClient::Client
  void cancel() override;
  void limit(Filters::Common::RatelimitClient::LimitRequestCallbacks& callbacks,
             const std::vector<std::shared_ptr<RateLimitPolicy>>& ratelimitpolicy, Tracing::Span& parent_span,
             const StreamInfo::StreamInfo& stream_info) override;
  void clean(Filters::Common::RatelimitClient::CleanRequestCallbacks& callbacks,
             const std::vector<std::string>& keys, Tracing::Span& parent_span,
             const StreamInfo::StreamInfo& stream_info) override;

  // Grpc::AsyncRequestCallbacks
  void onCreateInitialMetadata(Http::RequestHeaderMap&) override {}
  void onSuccessRaw(Buffer::InstancePtr&& response, Tracing::Span& span) override;
  void onFailure(Grpc::Status::GrpcStatus status, const std::string& message,
                 Tracing::Span& span) override;

private:
  // grpc服务名及方法名
  const static std::string serivce_name_;
  const static std::string limit_method_;
  const static std::string clean_method_;

  Grpc::RawAsyncClientSharedPtr client_;
  Grpc::AsyncRequest* request_{};
  absl::optional<std::chrono::milliseconds> timeout_;

  // 相关回调
  Filters::Common::RatelimitClient::LimitRequestCallbacks* limit_callbacks_{};
  Filters::Common::RatelimitClient::CleanRequestCallbacks* clean_callbacks_{};
};

/**
 * Builds the rate limit client.
 */
Filters::Common::RatelimitClient::ClientPtr
rateLimitClient(Server::Configuration::FactoryContext& context,
                const envoy::config::core::v3::GrpcService& grpc_service,
                const std::chrono::milliseconds timeout);

} // namespace Impl
} // namespace RatelimitClient
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
