#include <sstream>
#include <algorithm>
#include "database.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WeakPasswordCheck {
namespace Impl {

// weak_md5 table
const std::string WeakMd5Table::TableNameWeakMd5{"weak_md5"};
const std::string WeakMd5Table::WeakMd5FieldMd5{"md5"};
const std::string WeakMd5Table::WeakMd5FieldPlaintextPassword{"plaintext_password"};

// weak_password table
const std::string WeakPasswordTable::TableNameWeakPassword{"weak_password"};
const std::string WeakPasswordTable::WeakPasswordTableFieldPlainPassword{"plain_password"};
const std::string WeakPasswordTable::WeakPasswordTableFieldEncryptType{"encrypt_type"};
const std::string WeakPasswordTable::WeakPasswordTableFieldEncryptPassword{"encrypt_password"};
const std::string WeakPasswordTable::WeakPasswordTableFieldKey{"key"};

// rainbow db
const std::string RainbowDb::DbPathRainbow{"plugins/weak_password_check/conf/rainbow.db"};
const std::string RainbowDb::TableNameUserTable{"user_table"};
const std::string RainbowDb::UserTableFieldUserName{"username"};
const std::string RainbowDb::TableNamePasswordTable{"password_table"};
const std::string RainbowDb::PasswordTableFieldPassword{"password"};
const std::string RainbowDb::TableNameTokenTable{"token_table"};
const std::string RainbowDb::TokenTableFieldToken{"token"};

// custom_password db
const std::string CustomRainbowDb::DbPathCustomRainbow{"plugins/weak_password_check/conf/custom_password.db"};
const std::string CustomRainbowDb::TableNameRuleInfo{"rule_info"};
const std::string CustomRainbowDb::RuleInfoFieldId{"id"};
const std::string CustomRainbowDb::RuleInfoFieldInstanceId{"instance_id"};
const std::string CustomRainbowDb::RuleInfoFieldRuleHash{"rule_hash"};
const std::string CustomRainbowDb::RuleInfoFieldUpdateTime{"update_time"};
const std::string CustomRainbowDb::RuleInfoFieldPasswordTableName{"password_table_name"};
const std::string CustomRainbowDb::RuleInfoFieldHasUse{"has_use"};

SqliteDb::SqliteDb(const std::string& db_name) : db_name_(db_name) {
  if (SQLITE_OK != sqlite3_open(db_name_.c_str(), &db_)) {
    ENVOY_LOG(error, "open db {} failed", db_name_);
  } else {
    ENVOY_LOG(debug, "open db {} success", db_name_);
  }
}
SqliteDb::~SqliteDb() {
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

bool SqliteDb::executeSql(const std::string& sql, const std::string& query_field, std::vector<std::string>& results) {
  if (!db_) {
    return false;
  }

  ENVOY_LOG(debug, "sql: {}", sql);
  // 准备SQL查询
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    const char* errorMsg = sqlite3_errmsg(db_);
    ENVOY_LOG(error, errorMsg);
    return false;
  }
  // 执行查询并获取结果
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    // 获取查询结果中字段的值
    const char* field_value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    ENVOY_LOG(debug, "{}: {}", query_field, field_value);
    results.emplace_back(field_value);
  }
  // 释放资源
  sqlite3_finalize(stmt);
  return true;
}

bool SqliteDb::queryDb(const std::string& table_name, const std::string& query_field, std::vector<std::string>& results) {
  std::string sql = std::string("select ") + query_field + " from " + table_name + ";";
  return executeSql(sql, query_field, results);
}

bool SqliteDb::queryDb(const std::string& table_name, const std::string& query_field,
                       const std::string& match_field, const std::string& match_value, int limit,
                       std::vector<std::string>& results) {
  std::string sql = std::string("select ") + query_field + " from " + table_name + " where " +
                    match_field + " = '" + match_value + "' limit " + std::to_string(limit) + ";";
  return executeSql(sql, query_field, results);
}

bool SqliteDb::queryDb(const std::string& table_name, const std::string& query_field,
                               const std::string& match_field, const std::string& match_value,
                               std::string& result) {
  if (table_name.empty()) {
    return false;
  }
  std::vector<std::string> results;
  if (queryDb(table_name, query_field, match_field, match_value, 1, results)) {
    if (results.size()) {
      result = results[0];
      return true;
    }
  }
  return false;
}

bool SqliteDb::queryDb(const std::string& table_name, const std::string& query_field,
                               const std::string& match_field, uint64_t match_value,
                               std::string& result) {
  if (table_name.empty()) {
    return false;
  }
  std::vector<std::string> results;
  if (queryDb(table_name, query_field, match_field, std::to_string(static_cast<int64_t>(match_value)), 1, results)) {
    if (results.size()) {
      result = results[0];
      return true;
    }
  }
  return false;
}

bool SqliteDb::dropTable(const std::string& table_name) {
  if (!db_) {
    return false;
  }
  std::stringstream sql;
  sql << "drop table if exists " << table_name << ";";
  int rc = sqlite3_exec(db_, sql.str().c_str(), nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    ENVOY_LOG(debug, sqlite3_errmsg(db_));
    return false;
  }
  return true;
}


bool RainbowDb::queryWeakMd5Table(std::vector<struct WeakMd5Item>& items) {
  if (!db_) {
    return false;
  }

  std::ostringstream sql;
  sql << "select "
      << WeakMd5FieldPlaintextPassword << ","
      << WeakMd5FieldMd5 << " from "
      << TableNameWeakMd5 << ";";
  ENVOY_LOG(debug, "sql: {}", sql.str());

  // 准备SQL查询
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, sql.str().c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    const char* errorMsg = sqlite3_errmsg(db_);
    ENVOY_LOG(error, errorMsg);
    return false;
  }
  // 执行查询并获取结果
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    // 获取查询结果中字段的值
    struct WeakMd5Item item;
    item.plain_text_password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    item.md5 = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

    items.emplace_back(item);
  }
  // 释放资源
  sqlite3_finalize(stmt);
  return true;
}

bool RainbowDb::createWeakPasswordTable() {
  if (!db_ || weak_password_table_name_.empty()) {
    return false;
  }
  std::ostringstream sql;
  sql << "CREATE TABLE IF NOT EXISTS " << weak_password_table_name_ << "("
      << WeakPasswordTableFieldPlainPassword << " VARCHAR(255) NOT NULL, "
      << WeakPasswordTableFieldEncryptType << " INTEGER NOT NULL DEFAULT 0, "
      << WeakPasswordTableFieldEncryptPassword << " VARCHAR(255) NOT NULL, "
      << WeakPasswordTableFieldKey << " INTEGER PRIMARY KEY NOT NULL DEFAULT 0);";
  ENVOY_LOG(debug, "sql: {}", sql.str());
  int rc = sqlite3_exec(db_, sql.str().c_str(), 0, 0, 0);
  if (rc != SQLITE_OK) {
    ENVOY_LOG(debug, sqlite3_errmsg(db_));
    return false;
  }
  return true;
}

uint32_t RainbowDb::insertWeakPasswordTable(const std::vector<struct WeakPasswordItem>& items) {
  if (!db_ || weak_password_table_name_.empty()) {
    return 0;
  }
  std::stringstream sql;
  sql << "insert or ignore into " << weak_password_table_name_ << " ("
      << WeakPasswordTableFieldPlainPassword << ","
      << WeakPasswordTableFieldEncryptType << ","
      << WeakPasswordTableFieldEncryptPassword << ","
      << WeakPasswordTableFieldKey << ") values (?,?,?,?);";
  ENVOY_LOG(debug, "sql: {}", sql.str());
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, sql.str().c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    ENVOY_LOG(debug, sqlite3_errmsg(db_));
    return 0;
  }
  uint32_t insert_cnt = 0;
  // 开始事务
  sqlite3_exec(db_, "begin transaction;", nullptr, nullptr, nullptr);
  for (const auto& data : items) {
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    // 绑定参数
    sqlite3_bind_text(stmt, 1, data.plain_password.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, data.encrypt_type);
    sqlite3_bind_text(stmt, 3, data.encrypt_password.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, data.key);
    // 执行语句
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
      ENVOY_LOG(debug, sqlite3_errmsg(db_));
      continue;
    }
    insert_cnt++;
  }
  // 提交事务
  rc = sqlite3_exec(db_, "commit transaction;", nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    const char* errorMsg = sqlite3_errmsg(db_);
    ENVOY_LOG(error, errorMsg);
  }
  sqlite3_finalize(stmt);
  return insert_cnt;
}

std::string CustomRainbowDb::getWeakPasswordTableNameByHash(const std::string& rule_hash) {
  if (weak_password_table_name_.empty()) {
    queryDb(TableNameRuleInfo, RuleInfoFieldPasswordTableName, RuleInfoFieldRuleHash, rule_hash,
            weak_password_table_name_);
  }
  return weak_password_table_name_;
}

bool CustomRainbowDb::createRuleInfoTable() {
  if (!db_) {
    return false;
  }
  std::ostringstream sql;
  sql << "CREATE TABLE IF NOT EXISTS " << TableNameRuleInfo << "("
      << RuleInfoFieldId << " INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, "
      << RuleInfoFieldInstanceId << " VARCHAR(255) NOT NULL, "
      << RuleInfoFieldRuleHash << " CHAR(33) NOT NULL, "
      << RuleInfoFieldPasswordTableName << " CHAR(64) NOT NULL, "
      << RuleInfoFieldUpdateTime << " INT NOT NULL, "
      << RuleInfoFieldHasUse << " INT NOT NULL DEFAULT TRUE);";
  ENVOY_LOG(debug, "sql: {}", sql.str());
  int rc = sqlite3_exec(db_, sql.str().c_str(), 0, 0, 0);
  if (rc != SQLITE_OK) {
    ENVOY_LOG(debug, sqlite3_errmsg(db_));
    return false;
  }
  return true;
}

bool CustomRainbowDb::isRuleExist(const std::string& instance_id, const std::string& rule_hash) {
  bool ret = false;
  if (!db_) {
    return ret;
  }

  std::string sql = std::string("select ") + RuleInfoFieldId + " from " + TableNameRuleInfo + " where "
                    + RuleInfoFieldInstanceId + " = '" + instance_id + "' and "
                    + RuleInfoFieldRuleHash + " = '" + rule_hash + "';";

  ENVOY_LOG(debug, "sql: {}", sql);
  // 准备SQL查询
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    const char* errorMsg = sqlite3_errmsg(db_);
    ENVOY_LOG(error, errorMsg);
    return ret;
  }
  // 执行查询并获取结果
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    // 获取查询结果中字段的值
    int id = sqlite3_column_int(stmt, 0);
    ENVOY_LOG(debug, "{}: {}", RuleInfoFieldId, id);
    ret = true;
  }
  // 释放资源
  sqlite3_finalize(stmt);
  return ret;
}

bool CustomRainbowDb::updateExistRuleStatus(const std::string& instance_id, const std::string& rule_hash, bool status) {
  if (!db_) {
    return false;
  }
  std::string sql = std::string("update ") + TableNameRuleInfo + " set "
                    + RuleInfoFieldUpdateTime + " = '" + std::to_string(time(NULL)) + "', "
                    + RuleInfoFieldHasUse + " = " + RuleInfoFieldHasUse + (status ? "+1" : "-1") + " where "
                    + RuleInfoFieldInstanceId + " = '" + instance_id + "' and "
                    + RuleInfoFieldRuleHash + " = '" + rule_hash + "';";
  ENVOY_LOG(debug, "sql: {}", sql);
  int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    const char* errorMsg = sqlite3_errmsg(db_);
    ENVOY_LOG(error, errorMsg);
    return false;
  }
  return true;
}

bool CustomRainbowDb::deleteRule(int id) {
  if (!db_) {
    return false;
  }
  std::stringstream sql;
  sql << "delete from " << TableNameRuleInfo << " where " << RuleInfoFieldId << " = " << id << ";";
  int rc = sqlite3_exec(db_, sql.str().c_str(), nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    ENVOY_LOG(debug, sqlite3_errmsg(db_));
    return false;
  }
  return true;
}

bool CustomRainbowDb::insertRule(const std::string& instance_id, const std::string& rule_hash) {
  bool ret = false;
  if (!db_) {
    return ret;
  }

  std::string sql = std::string("insert into ") + TableNameRuleInfo + " (" +
                    RuleInfoFieldInstanceId + "," + RuleInfoFieldRuleHash + "," +
                    RuleInfoFieldPasswordTableName + "," + RuleInfoFieldUpdateTime + "," +
                    RuleInfoFieldHasUse + ") values (?,?,?,?,?);";
  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    ENVOY_LOG(debug, sqlite3_errmsg(db_));
    return ret;
  }
  // 开始事务
  sqlite3_exec(db_, "begin transaction;", nullptr, nullptr, nullptr);
  sqlite3_reset(stmt);
  sqlite3_clear_bindings(stmt);
  // 绑定参数
  sqlite3_bind_text(stmt, 1, instance_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, rule_hash.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, weak_password_table_name_.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 4, time(NULL));
  sqlite3_bind_int(stmt, 5, 1);

  // 执行语句
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    ENVOY_LOG(debug, sqlite3_errmsg(db_));
    ret = false;
  } else {
    ret = true;
  }
  // 提交事务
  sqlite3_exec(db_, "commit transaction;", nullptr, nullptr, nullptr);
  sqlite3_finalize(stmt);
  return ret;
}

bool CustomRainbowDb::queryRuleInfoTable(std::vector<struct RuleInfoTable>& rule_infos) {
  if (!db_) {
    return false;
  }

  std::string sql = std::string("select ") + RuleInfoFieldId + "," + RuleInfoFieldRuleHash + "," +
                    RuleInfoFieldHasUse + "," + RuleInfoFieldUpdateTime + "," +
                    RuleInfoFieldPasswordTableName + " from " + TableNameRuleInfo + ";";
  ENVOY_LOG(debug, "sql: {}", sql);

  // 准备SQL查询
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    const char* errorMsg = sqlite3_errmsg(db_);
    ENVOY_LOG(error, errorMsg);
    return false;
  }
  // 执行查询并获取结果
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    // 获取查询结果中字段的值

    int id = sqlite3_column_int(stmt, 0);
    std::string rule_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    int has_use = sqlite3_column_int(stmt, 2);
    time_t update_time = sqlite3_column_int(stmt, 3);
    std::string password_table_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    rule_infos.emplace_back(id, rule_hash, has_use, update_time, password_table_name);
  }
  // 释放资源
  sqlite3_finalize(stmt);
  return true;
}

bool CustomRainbowDb::deleteOldRules() {
  std::vector<struct RuleInfoTable> rule_infos;
  if (!queryRuleInfoTable(rule_infos)) {
    return false;
  }

  // step 1: find inuse_group and unuse_group
  std::set<std::string> inuse_set;
  std::set<std::string> unuse_set;
  for (const auto& it : rule_infos) {
    if (it.has_use_ > 0) {
      inuse_set.insert(it.rule_hash_);
    } else {
      unuse_set.insert(it.rule_hash_);
    }
  }
  // step 2: find can delete group
  std::set<std::string> deleteable_set;
  std::set_difference(unuse_set.begin(), unuse_set.end(), inuse_set.begin(), inuse_set.end(), std::insert_iterator<std::set<std::string>>(deleteable_set, deleteable_set.begin()));

  // step 3: find can delete record group
  std::vector<struct RuleInfoTable> deleteable_rule_infos;
  for (const auto& it : rule_infos) {
    if (deleteable_set.find(it.rule_hash_) != deleteable_set.end()) {
      deleteable_rule_infos.emplace_back(it);
      ENVOY_LOG(debug,
                "deleteable rule id: {}, rule_hash: {}, has_use: {}, update_time: {}, "
                "password_table_name: {}",
                it.id_, it.rule_hash_, it.has_use_, it.update_time_, it.password_table_name_);
    }
  }

  // step 4: sort and delete excceed records
  if (deleteable_rule_infos.size() >= UnuseRuleMaxNum) {
    std::sort(deleteable_rule_infos.begin(), deleteable_rule_infos.end(),
              [](const struct RuleInfoTable& lhs, const struct RuleInfoTable& rhs) {
                return lhs.update_time_ < rhs.update_time_;
              });
    while(deleteable_rule_infos.size() >= UnuseRuleMaxNum) {
      const auto& it = deleteable_rule_infos.front();
      if (deleteRule(it.id_)) {
        if (dropTable(it.password_table_name_)) {
          ENVOY_LOG(debug, "drop table {} success", it.password_table_name_);
        }
      } else {
        ENVOY_LOG(error, "delete rule_info failed, rule id: {}", it.id_);
      }
      deleteable_rule_infos.erase(deleteable_rule_infos.begin());
    }
  }

  return true;
}

} // namespace Impl
} // namespace WeakPasswordCheck
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy