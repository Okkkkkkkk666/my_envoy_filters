#include "source/common/srhino_plugin_framework/v1_0_x/data_slices_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/net/grpc_call_impl.h"
#include "grpc_call_impl.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace Net {

Envoy::Buffer::InstancePtr GrpcCallImpl::bufferizeData(const void* data, size_t size) {
  auto body = std::make_unique<Envoy::Buffer::OwnedImpl>();
  auto reservation = body->reserveSingleSlice(size);
  ASSERT(reservation.slice().len_ >= size);
  uint8_t* current = reinterpret_cast<uint8_t*>(reservation.slice().mem_);
  memcpy(current, data, size);
  reservation.commit(size);
  return body;
}

GrpcCallImpl::GrpcCallImpl(Envoy::Server::Configuration::FactoryContext& factory_context,
                           const std::string& cluster_name, const std::string& serivce_name)
    : cluster_name_(cluster_name), serivce_name_(serivce_name) {
  envoy::config::core::v3::GrpcService envoy_grpc;
  google::protobuf::Duration duration;
  duration.set_nanos(timeout_.count() * 1000);
  *(envoy_grpc.mutable_timeout()) = duration;
  envoy_grpc.mutable_envoy_grpc()->set_cluster_name(cluster_name_);
  client_ = factory_context.clusterManager().grpcAsyncClientManager().getOrCreateRawAsyncClient(
      envoy_grpc, factory_context.scope(), true, Envoy::Grpc::CacheOption::CacheWhenRuntimeEnabled);
}

GrpcCallImpl::~GrpcCallImpl() { cancel(); }

void GrpcCallImpl::send(const std::string& method, const DataSlices& data, GrpcCallback& cb) {
  const std::string& data_str = data.toString();
  send(method, data_str.data(), data_str.size(), cb);
}

void GrpcCallImpl::send(const std::string& method, const void* data, size_t size,
                        GrpcCallback& cb) {
  ASSERT(callbacks_ == nullptr);
  callbacks_ = &cb;
  request_ = client_->sendRaw(serivce_name_, method, bufferizeData(data, size), *this,
                              Envoy::Tracing::NullSpan::instance(),
                              Envoy::Http::AsyncClient::RequestOptions().setTimeout(timeout_));
}

void GrpcCallImpl::cancel() {
  if (callbacks_ == nullptr) {
    return;
  }
  request_->cancel();
  callbacks_ = nullptr;
}

void GrpcCallImpl::onSuccessRaw(Envoy::Buffer::InstancePtr&& response, Envoy::Tracing::Span&) {
  ENVOY_LOG(trace, "grpc request {}:{} success", cluster_name_, serivce_name_);
  if (callbacks_) {
    DataSlicesImpl data(*response);
    callbacks_->complete(GrpcCallback::Status::SUCCESS, data);
    callbacks_ = nullptr;
  }
}

void GrpcCallImpl::onFailure(Envoy::Grpc::Status::GrpcStatus status, const std::string& message,
                             Envoy::Tracing::Span&) {
  ENVOY_LOG(trace, "grpc request {}:{} failure, status:{},msg:{}", cluster_name_, serivce_name_,
            status, message);
  if (callbacks_) {
    callbacks_->complete(GrpcCallback::Status::FAILURE, DataSlicesImpl());
    callbacks_ = nullptr;
  }
}

} // namespace Net
// namespace Net
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework