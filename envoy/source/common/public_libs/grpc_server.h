#pragma once
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "envoy/grpc/async_client_manager.h"
#include "source/common/grpc/async_client_impl.h"
#include "source/common/grpc/common.h"
#include "source/common/common/logger.h"
#include "source/common/common/thread.h"
#include "envoy/server/instance.h"
#include "envoy/thread/thread.h"
#include "envoy/common/exception.h"
#include "envoy/server/instance.h"
#include "service_impl.h"

#include "source/common/srhino_plugin_framework/v1_4_x/libs/public_lib/cache_impl.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
namespace CacheImpl = SrhinoPluginFramework::v1_4_x::Libs::PublicLib;
class GrpcServerException : public Envoy::EnvoyException {
public:
  GrpcServerException(const std::string& message) : EnvoyException(message) {}
};

class GrpcServer : public Envoy::Logger::Loggable<Envoy::Logger::Id::main>,public Envoy::Grpc::AsyncRequestCallbacks<Proto::GetFullResponse>  {
public:
  GrpcServer(std::string_view service_address,Envoy::Server::Configuration::ServerFactoryContext& server);
  ~GrpcServer();

public:
  void run();

private:
  void close();
  void loop();
  void onSuccess(std::unique_ptr<Proto::GetFullResponse>&& response, Envoy::Tracing::Span& /*span*/) override;

  void onFailure(Envoy::Grpc::Status::GrpcStatus status, const std::string& message,
                 Envoy::Tracing::Span& /*span*/) override;
  void onCreateInitialMetadata(Envoy::Http::RequestHeaderMap&) override {}

private:
  CacheImpl::PublicLibCacheImplSharedPtr cache_impl_;

  bool is_running_{false};
  std::unique_ptr<grpc::Server> server_;
  std::string service_address_;
  std::unique_ptr<std::thread> thread_;
  std::chrono::milliseconds timeout_{1000};

  Envoy::Grpc::RawAsyncClientSharedPtr client_{nullptr};
  Envoy::Grpc::AsyncRequest* request_{nullptr};
  const std::string serivce_name_{
    "envoy.service.public_libs_service.v3.LibsService"};
  const std::string get_full_method_{"GetFull"};
  const std::string cluster_name_{"cluster.public_libs_service"};
};