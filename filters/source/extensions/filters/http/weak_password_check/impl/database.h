#pragma once
#include <string>

#include "source/common/common/logger.h"
#include "third-party/Sqlite3/sqlite3.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WeakPasswordCheck {
namespace Impl {


class SqliteDb : public Logger::Loggable<Logger::Id::filter> {
public:
  SqliteDb() = delete;
  SqliteDb(const std::string& db_name);
  virtual ~SqliteDb();

public:
  virtual const std::string& db_name() { return db_name_; }
  virtual bool executeSql(const std::string& sql, const std::string& query_field, std::vector<std::string>& results);
  virtual bool queryDb(const std::string& table_name, const std::string& query_field, std::vector<std::string>& results);
  virtual bool queryDb(const std::string& table_name, const std::string& query_field,
               const std::string& match_field, const std::string& match_value, int limit,
               std::vector<std::string>& results);
  virtual bool queryDb(const std::string& table_name, const std::string& query_field,
                       const std::string& match_field, const std::string& match_value,
                       std::string& result);
  virtual bool queryDb(const std::string& table_name, const std::string& query_field,
                       const std::string& match_field, uint64_t match_value,
                       std::string& result);
  virtual bool dropTable(const std::string& table_name);

protected:
  sqlite3* db_{nullptr};
private:
  const std::string db_name_;
};
using SqliteDbPtr = std::shared_ptr<SqliteDb>;

class WeakMd5Table {
public:
  static const std::string TableNameWeakMd5;
  static const std::string WeakMd5FieldMd5;
  static const std::string WeakMd5FieldPlaintextPassword;
};

class WeakPasswordTable {
public:
  static const std::string TableNameWeakPassword;
  static const std::string WeakPasswordTableFieldPlainPassword;
  static const std::string WeakPasswordTableFieldEncryptType;
  static const std::string WeakPasswordTableFieldEncryptPassword;
  static const std::string WeakPasswordTableFieldKey;
};

struct WeakMd5Item {
  std::string plain_text_password;
  std::string md5;
};

struct WeakPasswordItem {
  std::string plain_password;
  int encrypt_type;
  std::string encrypt_password;
  uint64_t key;
};

class RainbowDb : public SqliteDb, public WeakMd5Table, public WeakPasswordTable{
public:
  RainbowDb() : SqliteDb(DbPathRainbow), weak_password_table_name_(TableNameWeakPassword) {}
  RainbowDb(const std::string& db_name) : SqliteDb(db_name), weak_password_table_name_(std::string()) {}
  virtual ~RainbowDb() {};

public:
  uint32_t insertWeakPasswordTable(const std::vector<struct WeakPasswordItem>& items);
  std::string getWeakPasswordTableName() const { return weak_password_table_name_; }
  void setWeakPasswordTableName(const std::string& password_table_name) { weak_password_table_name_ = password_table_name; }
  bool getUserNames(std::vector<std::string>& user_names) {
    return queryDb(TableNameUserTable, UserTableFieldUserName, user_names);
  }
  bool getPasswordNames(std::vector<std::string>& password_names) {
    return queryDb(TableNamePasswordTable, PasswordTableFieldPassword, password_names);
  }
  bool getTokenNames(std::vector<std::string>& token_names) {
    return queryDb(TableNameTokenTable, TokenTableFieldToken, token_names);
  }
  bool getPlainPassword(uint64_t key, std::string& plain_password) {
    return queryDb(weak_password_table_name_, WeakPasswordTableFieldPlainPassword,
                   WeakPasswordTableFieldKey, key, plain_password);
  }

public:
  bool queryWeakMd5Table(std::vector<struct WeakMd5Item>& items);
  bool createWeakPasswordTable();

public:
  static const std::string DbPathRainbow;

  static const std::string TableNameUserTable;
  static const std::string UserTableFieldUserName;

  static const std::string TableNamePasswordTable;
  static const std::string PasswordTableFieldPassword;

  static const std::string TableNameTokenTable;
  static const std::string TokenTableFieldToken;

protected:
  std::string weak_password_table_name_;
};
using RainbowDbPtr = std::shared_ptr<RainbowDb>;


struct RuleInfoTable {
  int id_;
  std::string rule_hash_;
  int has_use_;
  time_t update_time_;
  std::string password_table_name_;
  RuleInfoTable(int id, const std::string& rule_hash, int has_use, time_t update_time,
                const std::string& password_table_name)
      : id_(id), rule_hash_(rule_hash), has_use_(has_use), update_time_(update_time),
        password_table_name_(password_table_name) {}
};

class CustomRainbowDb : public RainbowDb {
public:
  CustomRainbowDb() : RainbowDb(DbPathCustomRainbow) {}
  virtual ~CustomRainbowDb() {};

public:
  bool createRuleInfoTable();
  std::string getWeakPasswordTableNameByHash(const std::string& rule_hash);
  bool isRuleExist(const std::string& instance_id, const std::string& rule_hash);
  bool updateRuleInuse(const std::string& instance_id, const std::string& rule_hash) { return updateExistRuleStatus(instance_id, rule_hash, true); }
  bool updateRuleUnuse(const std::string& instance_id, const std::string& rule_hash) { return updateExistRuleStatus(instance_id, rule_hash, false); }
  bool deleteRule(int id);
  bool insertRule(const std::string& instance_id, const std::string& rule_hash);
  bool deleteOldRules();

private:
  bool updateExistRuleStatus(const std::string& instance_id, const std::string& rule_hash, bool status);
  bool queryRuleInfoTable(std::vector<struct RuleInfoTable>& rule_infos);

public:
  static const size_t UnuseRuleMaxNum{20};
  static const std::string DbPathCustomRainbow;
  static const std::string TableNameRuleInfo;
  static const std::string RuleInfoFieldId;
  static const std::string RuleInfoFieldInstanceId;
  static const std::string RuleInfoFieldRuleHash;
  static const std::string RuleInfoFieldUpdateTime;
  static const std::string RuleInfoFieldPasswordTableName;
  static const std::string RuleInfoFieldHasUse;

};
using CustomRainbowDbPtr = std::shared_ptr<CustomRainbowDb>;


} // namespace Impl
} // namespace WeakPasswordCheck
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy