#pragma once

#include <format>
#include "envoy/server/factory_context.h"
#include "envoy/srhino_plugin_framework/v1_1_x/libs/net/grpc_call.h"

#include "source/common/grpc/common.h"
#include "source/common/tracing/null_span_impl.h"
#include "source/common/grpc/typed_async_client.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Net {
/**
 * 注意，本类实现不是线程安全的。多线程环境下需要使用不同的实例。
 */
class GrpcCallImpl : public GrpcCall,
                     public Envoy::Grpc::RawAsyncRequestCallbacks,
                     public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
  friend class GrpcCallTest;

public:
  GrpcCallImpl(Envoy::Server::Configuration::FactoryContext& factory_context,
               const std::string& cluster_name, const std::string& serivce_name);
  ~GrpcCallImpl() override;

public:
  // GrpcCall
  void send(const std::string& method, const DataSlices& data, GrpcCallback& cb) override;
  void send(const std::string& method, const void* data, size_t size, GrpcCallback& cb) override;
  void cancel() override;

  // Envoy::Grpc::AsyncRequestCallbacks
  void onCreateInitialMetadata(Envoy::Http::RequestHeaderMap&) override {}
  void onSuccessRaw(Envoy::Buffer::InstancePtr&& response, Envoy::Tracing::Span& span) override;
  void onFailure(Envoy::Grpc::Status::GrpcStatus status, const std::string& message,
                 Envoy::Tracing::Span& span) override;

public:
  static Envoy::Buffer::InstancePtr bufferizeData(const void* data, size_t size);

private:
  // grpc服务名及方法名
  const std::string cluster_name_;
  const std::string serivce_name_;

  Envoy::Grpc::RawAsyncClientSharedPtr client_;
  Envoy::Grpc::AsyncRequest* request_{};
  std::chrono::milliseconds timeout_{1000};

  // 相关回调
  GrpcCallback* callbacks_{};
};
} // namespace Net
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework