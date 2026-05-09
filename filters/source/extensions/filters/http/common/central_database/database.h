#pragma once

#include <string>
#include <functional>
#include <chrono>

#include "envoy/server/factory_context.h"
#include "envoy/grpc/async_client_manager.h"

#include "filters/api/envoy/extensions/filters/http/common/central_database/v3/central_database.pb.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace CentralDatabase {
namespace dbv3 = envoy::extensions::filters::http::common::central_database::v3;

// 控制面为数据面提供集中式的K-V存储，使分布式数据面可以保持一致的状态。
// 本类实现了K-V存储的GRPC客户端。
// 注意：本类实例只支持单线程调用。多线程调用时，需构造不同实例。
class Database : public Grpc::RawAsyncRequestCallbacks,
                 public Logger::Loggable<Logger::Id::filter> {
public:
  Database(Server::Configuration::FactoryContext& context,
           const envoy::config::core::v3::GrpcService& grpc_service,
           const std::chrono::milliseconds& timeout, const std::string& name_space);

  // 同步接口
public:
  /**
   * 同步插入
   * @param key 键名
   * @param data 数据指针
   * @param size 数据长度
   * @param ttl 有效时长，单位秒
   * @param force 是否强制插入。当为真时，若已存在对应的键名则强制覆盖现有的值及时长。
   */
  void insert(const std::string& key, const void* data, size_t size, uint32_t ttl = 0,
              bool force = false);

  /**
   * 同步删除
   * @param key 键名
   */
  void del(const std::string& key);

  /**
   * 同步获取
   * @param key 键名
   * @param data 数据指针。如果此值不为nullptr则当对应键名不存在时进行插入。
   * @param size 进行插入时，指明数据长度
   * @param ttl 进行插入时，指明有效时长，单位秒
   */
  template <class VALUE_TYPE>
  VALUE_TYPE get(const std::string& /*key*/, const void* /*data = nullptr*/, size_t /*size = 0*/,
                 uint32_t /*ttl = 0*/) {
    throw "not implemented!";
  }

  /**
   * 同步更新
   * @param key 键名
   * @param data 数据指针
   * @param size 数据长度
   * @param ttl 有效时长，单位秒
   */
  void update(const std::string& key, const void* data, size_t size, uint32_t ttl = 0);

  // 异步接口
public:
  /**
   * 异步插入完成后的回调
   * @param result 成功或失败
   * @param key 键名
   * @param data 插入后的数据指针
   * @param size 插入后的数据长度
   */
  using InsertCallback = std::function<void(bool /*result*/, const std::string& /*key*/,
                                            const void* /*data*/, size_t /*size*/)>;

  /**
   * 异步删除完成后的回调
   * @param result 成功或失败
   * @param key 键名
   */
  using DelCallback = std::function<void(bool /*result*/, const std::string& /*key*/)>;

  /**
   * 异步获取完成后的回调
   * @param result 0-失败、1-获取成功、2-插入成功
   * @param key 键名
   * @param data 数据指针
   * @param size 数据长度
   */
  using GetCallback = std::function<void(int /*result*/, const std::string& /*key*/,
                                         const void* /*data*/, size_t /*size*/)>;

  /**
   * 异步更新完成后的回调
   * @param result 成功或失败
   * @param key 键名
   * @param data 更新后的数据指针
   * @param size 更新后的数据长度
   */
  using UpdateCallback = std::function<void(bool /*result*/, const std::string& /*key*/,
                                            const void* /*data*/, size_t /*size*/)>;

  /**
   * 异步清空完成后的回调
   * @param result 成功或失败
   */
  using CleanCallback = std::function<void(bool /*result*/)>;

  /**
   * 异步插入
   * @param key 键名
   * @param cb 异步插入完成后的回调
   * @param data 数据指针
   * @param size 数据长度
   * @param ttl 有效时长，单位秒
   * @param force 是否强制插入。当为真时，若已存在对应的键名则强制覆盖现有的值及时长。
   */
  void insertAsync(const std::string& key, InsertCallback cb, const void* data, size_t size,
                   uint32_t ttl = 0, bool force = false);

  /**
   * 异步删除
   * @param key 键名
   * @param cb 异步删除完成后的回调
   */
  void delAsync(const std::string& key, DelCallback cb);

  /**
   * 异步获取
   * @param key 键名
   * @param cb 异步获取完成后的回调
   * @param data 数据指针。如果此值不为nullptr则当对应键名不存在时进行插入。
   * @param size 进行插入时，指明数据长度
   * @param ttl 进行插入时，指明有效时长，单位秒
   */
  void getAsync(const std::string& key, GetCallback cb, const void* data = nullptr, size_t size = 0,
                uint32_t ttl = 0);

  /**
   * 异步更新
   * @param key 键名
   * @param cb 异步更新完成后的回调
   * @param data 数据指针
   * @param size 数据长度
   * @param ttl 有效时长，单位秒
   */
  void updateAsync(const std::string& key, UpdateCallback cb, const void* data, size_t size,
                   uint32_t ttl = 0);

  /**
   * 异步清空
   * @param cb 异步清空完成后的回调
   */
  void cleanAsync(CleanCallback cb);

  /**
   * 中止当前的异步请求
   */
  void cancel();

  // 基类Grpc::RawAsyncRequestCallbacks覆写
private:
  void onCreateInitialMetadata(Http::RequestHeaderMap&) {}
  void onSuccessRaw(Buffer::InstancePtr&& response, Tracing::Span& span) override;
  void onFailure(Grpc::Status::GrpcStatus status, const std::string& message,
                 Tracing::Span& span) override;

private:
  enum class State { Idle, CallingInsert, CallingDelete, CallingGet, CallingUpdate, CallingClean };

private:
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

  Grpc::RawAsyncClientSharedPtr client_;
  Envoy::Grpc::AsyncRequest* request_{nullptr};
  State state_{State::Idle};
};

using DatabasePtr = std::unique_ptr<Filters::Common::CentralDatabase::Database>;
} // namespace CentralDatabase
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy