#pragma once

#include "source/common/common/logger.h"
#include "sqlite3.h"
#include "envoy/srhino_plugin_framework/v1_4_x/libs/database/sqlite_database.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Database {

class SqliteDatabaseImpl : public SqliteDatabase,
                           public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  SqliteDatabaseImpl(const std::string& db_name);
  virtual ~SqliteDatabaseImpl();

public:
  bool execute(const std::string& sql, std::string& err_msg) override;
  bool insert(const std::string& sql, std::string& err_msg) override;
  bool query(const std::string& sql, void* p_res, SqliteQueryCb cb, std::string& err_msg) override;
  bool dropTable(const std::string& table_name, std::string& err_msg) override;

public:
  const std::string& db_name() override { return db_name_; }

private:
  const std::string db_name_;
  sqlite3* db_{nullptr};
};

} // namespace Database
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework
