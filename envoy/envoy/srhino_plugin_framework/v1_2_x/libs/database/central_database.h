#pragma once

#include <memory>
#include <string>

#include "envoy/srhino_plugin_framework/v1_2_x/context.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace Database {

// 控制面为数据面提供集中式的K-V存储，使分布式数据面可以保持一致的状态。
// 本类实现了K-V存储的GRPC客户端。
// 注意：本类实例只支持单线程调用。多线程调用时，需构造不同实例。
class CentralDatabase {
public:
  CentralDatabase() {}
  CentralDatabase(const CentralDatabase&) = delete;
  virtual ~CentralDatabase() = default;

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
  virtual void insert(const std::string& key, const void* data, size_t size, uint32_t ttl = 0,
                      bool force = false) = 0;

  /**
   * 同步删除
   * @param key 键名
   */
  virtual void del(const std::string& key) = 0;

  /**
   * 同步获取
   * @param key 键名
   * @param data 数据指针。如果此值不为nullptr则当对应键名不存在时进行插入。
   * @param size 进行插入时，指明数据长度
   * @param ttl 进行插入时，指明有效时长，单位秒
   */
  // template <class VALUE_TYPE>
  // VALUE_TYPE get(const std::string& /*key*/, const void* /*data = nullptr*/, size_t /*size = 0*/,
  //                uint32_t /*ttl = 0*/) {
  //   throw "not implemented!";
  // }

  /**
   * 同步更新
   * @param key 键名
   * @param data 数据指针
   * @param size 数据长度
   * @param ttl 有效时长，单位秒
   */
  virtual void update(const std::string& key, const void* data, size_t size, uint32_t ttl = 0) = 0;

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
  virtual void insertAsync(const std::string& key, InsertCallback cb, const void* data, size_t size,
                           uint32_t ttl = 0, bool force = false) = 0;

  /**
   * 异步删除
   * @param key 键名
   * @param cb 异步删除完成后的回调
   */
  virtual void delAsync(const std::string& key, DelCallback cb) = 0;

  /**
   * 异步获取
   * @param key 键名
   * @param cb 异步获取完成后的回调
   * @param data 数据指针。如果此值不为nullptr则当对应键名不存在时进行插入。
   * @param size 进行插入时，指明数据长度
   * @param ttl 进行插入时，指明有效时长，单位秒
   */
  virtual void getAsync(const std::string& key, GetCallback cb, const void* data = nullptr,
                        size_t size = 0, uint32_t ttl = 0) = 0;

  /**
   * 异步更新
   * @param key 键名
   * @param cb 异步更新完成后的回调
   * @param data 数据指针
   * @param size 数据长度
   * @param ttl 有效时长，单位秒
   */
  virtual void updateAsync(const std::string& key, UpdateCallback cb, const void* data, size_t size,
                           uint32_t ttl = 0) = 0;

  /**
   * 异步清空
   * @param cb 异步清空完成后的回调
   */
  virtual void cleanAsync(CleanCallback cb) = 0;

  /**
   * 中止当前的异步请求
   */
  virtual void cancel() = 0;
};
using CentralDatabaseSharedPtr = std::shared_ptr<CentralDatabase>;

} // namespace Database
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework