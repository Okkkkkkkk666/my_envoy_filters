#pragma once

#include <memory>
#include <string>

#include "central_database.h"
#include "sqlite_database.h"
#include "ip_region_search.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
namespace Database {

class Database {
public:
  Database() {}
  Database(const Database&) = delete;
  virtual ~Database() = default;

public:
  /**
   * 创建一个中央缓存实例
   * @return 返回中央缓存实例。在触发onInstall时由于尚未初始化完毕，此时会返回nullptr。
   */
  virtual CentralDatabaseSharedPtr createCentralDatabase(const std::chrono::milliseconds& timeout,
                                                         const std::string& name_space) = 0;
  /**
   * 创建一个sqlite数据库连接实例
   * @return 返回sqlite数据库连接实例。在触发onInstall时由于尚未初始化完毕，此时会返回nullptr。
   */
  virtual SqliteDatabaseSharedPtr createSqliteDatabase(const std::string& db_name) = 0;
  /**
   * 创建一个查询ip归属地实例
   * @return 返回查询ip归属地实例。在触发onInstall时由于尚未初始化完毕，此时会返回nullptr。
   */
  virtual IpRegionSearchSharedPtr createIpRegionSearch(const std::string& xdb_name) = 0;
};
using DatabaseSharedPtr = std::shared_ptr<Database>;

} // namespace Database
} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework