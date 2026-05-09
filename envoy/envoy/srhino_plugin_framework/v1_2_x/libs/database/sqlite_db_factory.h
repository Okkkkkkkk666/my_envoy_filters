#pragma once

#include <memory>
#include <string>

#include "sqlite_database.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace Database {

class SqliteDbFactory {
public:
  SqliteDbFactory() {}
  SqliteDbFactory(const SqliteDbFactory&) = delete;
  virtual ~SqliteDbFactory() = default;

public:
  /**
   * 创建一个sqlite数据库连接实例
   * @return 返回sqlite数据库连接实例。在触发onInstall时由于尚未初始化完毕，此时会返回nullptr。
   */
  virtual SqliteDatabaseSharedPtr createSqliteDatabase(const std::string& db_name) = 0;
};
using SqliteDbFactorySharedPtr = std::shared_ptr<SqliteDbFactory>;

} // namespace Database
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework