#include <future>

#include "source/common/common/logger.h"
#include "filters/source/extensions/filters/http/common/lru_cache.hpp"
#include "filters/source/extensions/filters/http/common/central_database/database.h"


namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace UserIdentify {
namespace Impl {
namespace CentralDatabase = Filters::Common::CentralDatabase;

#define LOGIN_USER_INFO_TTL  (86400)
#define GUEST_USER_INFO_TTL  (20)
#define USER_NAME_MAX_LEN    (128)

struct UserInfo {
  char user_name_[USER_NAME_MAX_LEN + 1];
  int64_t     time_stamp_{0};

  UserInfo(const std::string& user_name, int64_t time_stamp) : time_stamp_(time_stamp) {
    size_t cpy_len = (user_name.size() > USER_NAME_MAX_LEN) ? USER_NAME_MAX_LEN : user_name.size();
    memset(user_name_, 0, sizeof(user_name_));
    memcpy(user_name_, user_name.data(), cpy_len);
  }

  // 不比较超时时间，只比较其它字段
  bool operator==(const UserInfo& ui) const {
    return 0 == memcmp(ui.user_name_, this->user_name_, sizeof(ui.user_name_));
  }
  bool operator!=(const UserInfo& ui) const { return !(*this == ui); }
};

/**
 * 存在问题，当中央缓存连不上时，无法正常工作
*/
class DataInteractive : public Logger::Loggable<Logger::Id::filter> {
public:
  DataInteractive() {}
  bool searchLocalCache(const std::string& token, std::string& user_name);
  bool updateLocalCache(const std::string& token, const UserInfo* p_new_user_info);
  bool searchAndUpdateLocalCache(const std::string& token, const std::string& insert_user_name,
                                 std::string& user_name, CentralDatabase::DatabasePtr& kv_db,
                                 bool& need_sync, std::function<void()> cb);

  // bool searchCache(const std::string& token, std::string& user_name,
  //                  CentralDatabase::DatabasePtr& kv_db, bool& need_sync, std::function<void()> cb);
  bool updateCache(const std::string& token, const std::string& username,
                   CentralDatabase::DatabasePtr& kv_db, bool& need_sync, std::function<void()> cb);

private:
  static int64_t now();
  bool expired(const UserInfo& ui) const;
  Filters::Common::LruCache<std::string /*token*/, UserInfo /*user info*/,
                            20011 /*must be prime number*/>
      user_info_cache_;
  };
} // namespace Impl
} // namespace UserIdentify
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy