#include "source/common/common/assert.h"
#include "grpc_server.h"

GrpcServer::~GrpcServer() {
  close();
  if (request_) {
    request_->cancel();
    request_ = nullptr;
  }
  if (thread_ && thread_->joinable()) {
    thread_->join();
  }
}

GrpcServer::GrpcServer(std::string_view service_address,
                       Envoy::Server::Configuration::ServerFactoryContext& server)
    : service_address_(service_address) {
  cache_impl_ = std::make_shared<CacheImpl::PublicLibCacheImpl>();
  auto channel = grpc::CreateChannel(service_address_, grpc::InsecureChannelCredentials());
  envoy::config::core::v3::GrpcService envoy_grpc;
  google::protobuf::Duration duration;
  duration.set_nanos(1000);
  *(envoy_grpc.mutable_timeout()) = duration;
  envoy_grpc.mutable_envoy_grpc()->set_cluster_name(cluster_name_);
  client_ = server.clusterManager().grpcAsyncClientManager().getOrCreateRawAsyncClient(
      envoy_grpc, server.scope(), true, Envoy::Grpc::CacheOption::CacheWhenRuntimeEnabled);
  ENVOY_LOG(info, "send grpc msg");
  google::protobuf::Empty empty_request;
  request_ = client_->sendRaw(serivce_name_, get_full_method_,
                              Envoy::Grpc::Common::serializeMessage(empty_request), *this,
                              Envoy::Tracing::NullSpan::instance(),
                              Envoy::Http::AsyncClient::RequestOptions().setTimeout(timeout_));
}

void GrpcServer::onSuccess(std::unique_ptr<Proto::GetFullResponse>&& response,
                           Envoy::Tracing::Span& /*span*/) {
  //全量获取
  request_ = nullptr;
  for(auto& policy:response->policy()){
    std::string policy_name = policy.policy_name();
    ENVOY_LOG(info,"policy: {}",policy_name);
    for(auto& rule:policy.rule()){
      ENVOY_LOG(info,"add full rule {},data is {}",rule.rule_id(),rule.data());
      cache_impl_->updateRule(policy_name,rule.rule_id(),rule.data());
    }
  }
}

void GrpcServer::onFailure(Envoy::Grpc::Status::GrpcStatus status, const std::string& message,
                           Envoy::Tracing::Span& /*span*/) {
  request_ = nullptr;
  ENVOY_LOG(error, "getfull fail, msg is {}, status is {}", message, status);
}

void GrpcServer::run() {
  ASSERT(is_running_ == false);
  try {
    thread_ = std::make_unique<std::thread>([this]() { this->loop(); });
  } catch (const std::exception& e) {
    throw GrpcServerException(fmt::format("Failed to start public_libs service: {}", e.what()));
  }
}

void GrpcServer::loop() {
  pthread_setname_np(pthread_self(), "public_libs_service");
  LibsServiceImpl service;
  grpc::ServerBuilder builder;
  builder.AddListeningPort(service_address_, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  server_ = builder.BuildAndStart();
  is_running_ = true;
  ENVOY_LOG(info, "public_libs grpc service bind on {}", service_address_);
  server_->Wait();
  ENVOY_LOG(info, "public_libs_service thread exit");
}

void GrpcServer::close() {
  if (is_running_ && server_) {
    ENVOY_LOG(info, "Stop public_libs grpc service");
    server_->Shutdown();
    is_running_ = false;
  }
}