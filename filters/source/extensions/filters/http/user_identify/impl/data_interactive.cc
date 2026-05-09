#include "data_interactive.h"
namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace UserIdentify {
namespace Impl {

// 从本地缓存查询信息
bool DataInteractive::searchLocalCache(const std::string& token, std::string& user_name) {
  bool is_find = false;
  user_info_cache_.peek(token, [&](const UserInfo* ui) {
    if (ui) {
      ENVOY_LOG(trace, "local cache(token={}), find userinfo", token);
      if (!expired(*ui)) {
        ENVOY_LOG(trace, "userinfo not expired, user_name:{}, time_stamp_:{}, now:{}, remain:{}",
                  ui->user_name_, ui->time_stamp_, now(), ui->time_stamp_ - now());
        user_name = ui->user_name_;
        is_find = true;
      } else {
        ENVOY_LOG(trace, "userinfo expired, user_name:{}, time_stamp_:{}, now:{}, excceed:{}",
                  ui->user_name_, ui->time_stamp_, now(),  now() - ui->time_stamp_);
      }
    } else {
      ENVOY_LOG(trace, "lookup local cache(key={}),did not find userinfo", token);
    }
  });
  return is_find;
}

/**
 * 更新本地缓存
 * return: true 更新
 *         false 不需要更新（未更新)
 */
bool DataInteractive::updateLocalCache(const std::string& token, const UserInfo* p_new_user_info) {
  bool is_update = false;

  user_info_cache_.access(
      token,
      [&](UserInfo& ui) {
        if (expired(ui) || ui != *p_new_user_info) {
          is_update = true;
          ENVOY_LOG(
              debug,
              "update local cache. user_name:{} -> {}, time_stamp_:{}(remain:{}) -> {}(remain:{})",
              ui.user_name_, p_new_user_info->user_name_, ui.time_stamp_,
              ui.time_stamp_ - now(), p_new_user_info->time_stamp_,
              p_new_user_info->time_stamp_ - now());
          ui = *p_new_user_info;
        }
      },
      [&]() {
        is_update = true;
        ENVOY_LOG(debug, "insert local cache. user_name:{}, time_stamp_:{}(remain:{})",
                  p_new_user_info->user_name_, p_new_user_info->time_stamp_,
                  p_new_user_info->time_stamp_ - now());
        return *p_new_user_info;
      });
  return is_update;
}

/**
 * 从缓存查询用户名
 * 先从本地缓存查询，如果查询到返回true；
 * param(in) token
 * param(in) kv_db
 * param(in) cb
 * param(out) user_name
 * param(out) need_sync 
 * 
 * return true 
*/
// bool DataInteractive::searchCache(const std::string& token, std::string& user_name,
//                                   CentralDatabase::DatabasePtr& kv_db, bool& need_sync,
//                                   std::function<void()> cb) {
//   if (searchLocalCache(token, user_name)) {
//     return true;
//   }

//   // 同步缓存
//   if (kv_db) {
//     need_sync = true;
//     ENVOY_LOG(trace, "pull the remote cache");
//     kv_db->getAsync(
//         token, [&, cb](int result, const std::string& key, const void* value, size_t size) {
//           ENVOY_LOG(debug, "pull complete. result:{} key:{} size:{}", result, key, size);
//           if (result > 0) {
//             user_name = std::string(static_cast<const char*>(value), size);
//             user_info_cache_.access(
//                 key,
//                 [&](std::string& username) {
//                   username = std::string(static_cast<const char*>(value), size);
//                   ENVOY_LOG(debug, "update username from local cache");
//                 },
//                 [&]() {
//                   ENVOY_LOG(debug, "insert username into local cache");
//                   return std::string(static_cast<const char*>(value), size);
//                 });
//           }
//           cb();
//         });
//   }
//   return false;
// }

int64_t DataInteractive::now() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

bool DataInteractive::expired(const UserInfo& ui) const {
  if (ui.user_name_[0] == 0 || ui.time_stamp_ == 0) {
    return true;
  }

  return now() > ui.time_stamp_;
}

/**
 * guest token的处理
 * 查询本地缓存，如果找到设置user_name返回
 *     如果中央缓存查找成功，则更新到本地缓存
 *     如果中央缓存未找到就使用insert_user_name更新本地缓存
 * 从中央缓存查询，由于是异步，因此立即返回false，且设置need_sync=true；中央缓存查询成功，则更新本地缓存
 * 当调用者看到need_sync为true时，应该中断迭代器;当中央缓存响应时，cb回调将被调用，调用者可在cb中恢复迭代器
*/
bool DataInteractive::searchAndUpdateLocalCache(const std::string& token,
                                                const std::string& insert_user_name,
                                                std::string& user_name,
                                                CentralDatabase::DatabasePtr& kv_db,
                                                bool& need_sync, std::function<void()> cb) {
  if (searchLocalCache(token, user_name)) {
    return true;
  }

  ENVOY_LOG(debug, "insert_user_name:{}", insert_user_name);
  // 同步缓存
  if (kv_db) {
    need_sync = true;
    ENVOY_LOG(trace, "pull the remote cache");
    kv_db->getAsync(
        token, [&, cb](int result, const std::string& key, const void* value, size_t size) {
          ENVOY_LOG(debug, "pull complete. result:{} key:{} size:{}", result, key, size);
          if (result > 0) {
            ASSERT(sizeof(UserInfo) == size);
            if (sizeof(UserInfo) == size) {
              updateLocalCache(token, reinterpret_cast<const UserInfo*>(value));
            }
          } else {
            // 获取失败，则更新本地缓存
            UserInfo new_user_info(insert_user_name, now() + GUEST_USER_INFO_TTL);
            updateLocalCache(token, &new_user_info);
          }
          cb();
        });
  }
  return false;
}

/**
 * login token的处理
 * 更新缓存
 * 首先查询本地缓存，如果找到且验证成功，则返回true
 * 否则，则插入数据到中央缓存，中央缓存插入成功后，再将数据插入到本地缓存
 * 
*/
bool DataInteractive::updateCache(const std::string& token, const std::string& user_name,
                                  CentralDatabase::DatabasePtr& kv_db, bool& need_sync,
                                  std::function<void()> cb) {
  UserInfo new_user_info(user_name, now() + LOGIN_USER_INFO_TTL);

  if (!updateLocalCache(token, &new_user_info)) {
    return true;
  }

  if (kv_db) {
    need_sync = true;
    ENVOY_LOG(trace, "push the local cache to remote");
    kv_db->getAsync(
        token,
        [&, cb](int result, const std::string& key, const void* value, size_t size) {
          ENVOY_LOG(debug, "push complete. result:{} key:{} size:{}", result, key, size);
          if (result > 0) {
            ASSERT(sizeof(UserInfo) == size);
            if (sizeof(UserInfo) == size) {
              updateLocalCache(token, reinterpret_cast<const UserInfo*>(value));
            }
          } else {
            // 获取失败，前面已经更新过了，不用处理
          }
          cb();
        },
        &new_user_info, sizeof(new_user_info));
  }
  return false;
}

} // namespace Impl
} // namespace UserIdentify
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy