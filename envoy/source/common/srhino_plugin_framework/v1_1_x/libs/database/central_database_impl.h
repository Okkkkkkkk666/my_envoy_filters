#pragma once
#include "envoy/grpc/async_client_manager.h"
#include "envoy/server/factory_context.h"

#include "envoy/srhino_plugin_framework/v1_1_x/libs/database/central_database.h"
#include "envoy/srhino_plugin_framework/v1_1_x/proto/database/central_database.pb.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Database {

namespace dbv3 = srhino_plugin_framework::v1_1_x::proto::database::central_database;

class CentralDatabaseImpl : public CentralDatabase,
                            public Envoy::Grpc::RawAsyncRequestCallbacks,
                            public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  CentralDatabaseImpl(Envoy::Server::Configuration::FactoryContext& factory_context, const std::chrono::milliseconds& timeout,
                      const std::string& name_space);
  CentralDatabaseImpl(const CentralDatabaseImpl&) = delete;
  virtual ~CentralDatabaseImpl() = default;

public:
  void insert(const std::string& key, const void* data, size_t size, uint32_t ttl = 0,
              bool force = false) override;
  void del(const std::string& key) override;
  void update(const std::string& key, const void* data, size_t size, uint32_t ttl = 0) override;

public:
  void insertAsync(const std::string& key, InsertCallback cb, const void* data, size_t size,
                   uint32_t ttl, bool force) override;
  void delAsync(const std::string& key, DelCallback cb) override;
  void getAsync(const std::string& key, GetCallback cb, const void* data = nullptr, size_t size = 0,
                uint32_t ttl = 0) override;
  void updateAsync(const std::string& key, UpdateCallback cb, const void* data, size_t size,
                   uint32_t ttl = 0) override;
  void cleanAsync(CleanCallback cb) override;
  void cancel() override;
  // 基类Grpc::RawAsyncRequestCallbacks覆写
private:
  void onCreateInitialMetadata(Envoy::Http::RequestHeaderMap&) {}
  void onSuccessRaw([[maybe_unused]] Envoy::Buffer::InstancePtr&& response, [[maybe_unused]] Envoy::Tracing::Span& span) override;
  void onFailure([[maybe_unused]] Envoy::Grpc::Status::GrpcStatus status, [[maybe_unused]] const std::string& message,
                 [[maybe_unused]] Envoy::Tracing::Span& span) override;

private:
  enum class State { Idle, CallingInsert, CallingDelete, CallingGet, CallingUpdate, CallingClean };

private:
  // cluster
  const static std::string cluster_name_;
  // 命名空间
  std::string namespace_;
  // 各个请求的超时时间
  std::chrono::milliseconds timeout_;

  // 相关回调
  InsertCallback insert_cb_;
  DelCallback delete_cb_;
  GetCallback get_cb_;
  UpdateCallback update_cb_;
  CleanCallback clean_cb_;

  // 请求消息缓存，回调时使用
  dbv3::InsertRequest insert_req_msg_;
  dbv3::DeleteRequest del_req_msg_;
  dbv3::GetRequest get_req_msg_;
  dbv3::UpdateRequest update_req_msg_;
  dbv3::CleanRequest clean_req_msg_;

  // grpc服务名及方法名
  const static std::string serivce_name_;
  const static std::string insert_method_;
  const static std::string del_method_;
  const static std::string get_method_;
  const static std::string update_method_;
  const static std::string clean_method_;

  Envoy::Grpc::RawAsyncClientSharedPtr client_{nullptr};
  Envoy::Grpc::AsyncRequest* request_{nullptr};
  State state_{State::Idle};
};

} // namespace Database
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework