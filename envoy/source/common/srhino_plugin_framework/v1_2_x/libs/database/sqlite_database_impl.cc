#include <sstream>
#include <iostream>
#include "source/common/srhino_plugin_framework/v1_2_x/libs/database/sqlite_database_impl.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace Database {

SqliteDatabaseImpl::SqliteDatabaseImpl(const std::string& db_name) : db_name_(db_name) {
  if (SQLITE_OK != sqlite3_open(db_name_.c_str(), &db_)) {
    ENVOY_LOG(error, "open db {} failed", db_name_);
  } else {
    ENVOY_LOG(debug, "open db {} success", db_name_);
  }
}

SqliteDatabaseImpl::~SqliteDatabaseImpl() {
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
    ENVOY_LOG(debug, "close db {} success", db_name_);
  }
}

bool SqliteDatabaseImpl::execute(const std::string& sql, std::string& err_msg) {
  if (!db_) {
    return false;
  }
  int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    err_msg = sqlite3_errmsg(db_);
    return false;
  }
  return true;
}

bool SqliteDatabaseImpl::insert(const std::string& sql, std::string& err_msg) {
  return execute(sql, err_msg);
}

bool SqliteDatabaseImpl::query(const std::string& sql, void* p_res, SqliteQueryCb cb, std::string& err_msg) {
  if (!db_ || !p_res || !cb) {
    return false;
  }
  
  int rc = sqlite3_exec(db_, sql.c_str(), cb, p_res, nullptr);
  if (rc != SQLITE_OK) {
    err_msg = sqlite3_errmsg(db_);
    return false;
  }
  return true;
}

bool SqliteDatabaseImpl::dropTable(const std::string& table_name, std::string& err_msg) {
  std::stringstream sql;
  sql << "DROP TABLE IF EXISTS " << table_name << ";";
  return execute(sql.str(), err_msg);
}


} // namespace Database
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework