#pragma once
#include <grpc++/grpc++.h>

#include "envoy/service/public_libs/v3/public_libs_service.grpc.pb.h"
#include "envoy/service/public_libs/v3/public_libs_service.pb.h"
#include "source/common/common/logger.h"
#include "source/common/srhino_plugin_framework/v1_4_x/libs/public_lib/cache_impl.h"

namespace Proto = envoy::service::public_libs_service::v3;
namespace CacheImpl = SrhinoPluginFramework::v1_4_x::Libs::PublicLib;
class LibsServiceImpl : public Proto::LibsService::Service,
                        public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  LibsServiceImpl(){
    cache_impl_ = std::make_shared<CacheImpl::PublicLibCacheImpl>();
  }                        
  grpc::Status Delete(grpc::ServerContext* /*context*/, const Proto::Request* request,
                      Proto::Response* response) override;

  grpc::Status Update(grpc::ServerContext* /*context*/, const Proto::Request* request,
                      Proto::Response* response) override;

  grpc::Status AddFull(grpc::ServerContext* /*context*/, const Proto::Request* request,
                      Proto::Response* response) override;
private:
  CacheImpl::PublicLibCacheImplSharedPtr cache_impl_;
};