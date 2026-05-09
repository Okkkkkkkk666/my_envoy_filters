#include <filesystem>

#include "source/common/srhino_plugin_framework/v1_0_x/libs/database/sqlite_database_impl.h"

#include "gmock/gmock.h"

namespace TestSrhinoPluginFrameWork {
namespace v1_0_x {
namespace Libs {
namespace Database {
namespace {

using namespace SrhinoPluginFramework::v1_0_x::Libs::Database;

class SqliteDatabaseTest : public testing::Test {
public:
  void SetUp(std::string db_path) {
    db_path_ = db_path;
    EXPECT_FALSE(std::filesystem::exists(db_path_));
    db_ = std::make_shared<SqliteDatabaseImpl>(db_path_);
    EXPECT_EQ(db_->db_name(), db_path_);
  }

  void TearDown() {
    db_.reset();
    std::filesystem::remove(db_path_);
  }

  void CreateTable(std::string table_name) {
    std::stringstream sql;
    sql << "CREATE TABLE " << table_name
        << "('test_field' varchar(255) NOT NULL DEFAULT '');";
    std::string err_msg;
    auto status = db_->execute(sql.str(), err_msg);
    EXPECT_TRUE(status);
    EXPECT_TRUE(err_msg.empty());
  }

  std::string GenerateDbPath() {
    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()
    );
    return std::format("/tmp/srhino_plugin_framework_sqlite_database_test_{}.db", ms.count());
  }

public:
  std::string db_path_;
  std::shared_ptr<SqliteDatabaseImpl> db_;
};

auto fetch_strings_callback = [](void *p_data, int num_field, char **p_fields, char **p_col_names) {
  if (num_field != 1) {
    return -1;
  }
  (void)p_col_names;
  auto *p_res = reinterpret_cast<std::vector<std::string> *>(p_data);
  p_res->emplace_back(p_fields[0]);
  return 0;
};


TEST_F(SqliteDatabaseTest, CreateDatabase) {
  std::string db_path = GenerateDbPath();
  SetUp(db_path);

  EXPECT_EQ(db_->db_name(), db_path);
  EXPECT_TRUE(std::filesystem::exists(db_path));
}

TEST_F(SqliteDatabaseTest, CreateTable) {
  SetUp(GenerateDbPath());
  std::string table_name = "test_table";
  CreateTable(table_name);

  std::string err_msg;
  std::vector<std::string> result;
  std::string query_string = "select name from sqlite_master where type='table';";
  auto res = db_->query(query_string, static_cast<void *>(&result),
                        fetch_strings_callback, err_msg);
  EXPECT_TRUE(res);
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result[0], table_name);
  EXPECT_TRUE(err_msg.empty());
}

TEST_F(SqliteDatabaseTest, ExecuteSuccess) {
  SetUp(GenerateDbPath());
  std::string table_name = "test_table";
  CreateTable(table_name);

  std::string query_string = "select name from sqlite_master where type='table';";
  std::string err_msg;
  auto res = db_->execute(query_string, err_msg);
  EXPECT_TRUE(res);
  EXPECT_TRUE(err_msg.empty());
}

TEST_F(SqliteDatabaseTest, ExecuteFailure) {
  SetUp(GenerateDbPath());
  std::string table_name = "test_table";
  CreateTable(table_name);

  std::string error_sql = "error sql query;";
  std::string err_msg;
  auto res = db_->execute(error_sql, err_msg);
  EXPECT_FALSE(res);
  EXPECT_FALSE(err_msg.empty());
  EXPECT_EQ(err_msg, "near \"error\": syntax error");
}

TEST_F(SqliteDatabaseTest, InsertAndQuery) {
  SetUp(GenerateDbPath());
  std::string table_name = "test_table";
  CreateTable(table_name);

  constexpr std::string_view insert_template = "insert into {} (test_field) values ('{}');";
  std::string insert_value1 = "value1";
  std::string insert_sql1 = std::format(insert_template, table_name, insert_value1);
  std::string err_msg;
  auto res = db_->insert(insert_sql1, err_msg);
  EXPECT_TRUE(res);
  EXPECT_TRUE(err_msg.empty());

  std::string query_sql = std::format("select test_field from {};", table_name);
  std::vector<std::string> result;
  err_msg.clear();
  res = db_->query(query_sql, static_cast<void *>(&result),
                   fetch_strings_callback, err_msg);
  EXPECT_TRUE(res);
  EXPECT_TRUE(err_msg.empty());
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result[0], insert_value1);

  std::string insert_value2 = "value2 value2";
  std::string insert_sql2 = std::format(insert_template, table_name, insert_value2);
  err_msg.clear();
  res = db_->insert(insert_sql2, err_msg);
  EXPECT_TRUE(res);
  EXPECT_TRUE(err_msg.empty());

  err_msg.clear();
  result.clear();
  res = db_->query(query_sql, static_cast<void *>(&result),
                   fetch_strings_callback, err_msg);
  EXPECT_TRUE(res);
  EXPECT_TRUE(err_msg.empty());
  EXPECT_EQ(result.size(), 2);
  EXPECT_EQ(result[0], insert_value1);
  EXPECT_EQ(result[1], insert_value2);
}

TEST_F(SqliteDatabaseTest, BatchInsertAndQuery) {
  SetUp(GenerateDbPath());
  std::string table_name = "test_table";
  CreateTable(table_name);

  std::stringstream batch_insert_sql;
  const int count = 20000;
  batch_insert_sql << "insert into " << table_name << " (test_field) values ";
  for (int i = 0; i < count; ++i) {
    batch_insert_sql << "('string value " << i << "')";
    if (i != count - 1) {
      batch_insert_sql << ",";
    }
  }
  batch_insert_sql << ";";
  std::string err_msg;
  auto res = db_->insert(batch_insert_sql.str(), err_msg);
  EXPECT_TRUE(res);
  EXPECT_TRUE(err_msg.empty());

  std::string query_sql = std::format("select test_field from {};", table_name);
  std::vector<std::string> result;
  res = db_->query(query_sql, static_cast<void *>(&result),
                   fetch_strings_callback, err_msg);
  EXPECT_TRUE(res);
  EXPECT_TRUE(err_msg.empty());
  EXPECT_EQ(result.size(), count);
  for (int i = 0; i < count; ++i) {
    std::string expect = std::format("string value {}", i);
    EXPECT_EQ(result[i], expect);
  }
}

TEST_F(SqliteDatabaseTest, QueryEmpty) {
  SetUp(GenerateDbPath());
  std::string table_name = "test_table";
  CreateTable(table_name);

  std::string query_sql = std::format("select test_field from {};", table_name);
  std::vector<std::string> result;
  std::string err_msg;
  auto res = db_->query(query_sql, static_cast<void *>(&result),
                        fetch_strings_callback, err_msg);
  EXPECT_TRUE(res);
  EXPECT_TRUE(err_msg.empty());
  EXPECT_EQ(result.size(), 0);
}

TEST_F(SqliteDatabaseTest, QueryFailure) {
  SetUp(GenerateDbPath());
  std::string table_name = "test_table";
  CreateTable(table_name);

  std::string query_sql = std::format("select error_field from {};", table_name);
  std::vector<std::string> result;
  std::string err_msg;
  auto res = db_->query(query_sql, static_cast<void *>(&result),
                        fetch_strings_callback, err_msg);
  EXPECT_FALSE(res);
  EXPECT_FALSE(err_msg.empty());
  EXPECT_EQ(err_msg, "no such column: error_field");
  EXPECT_EQ(result.size(), 0);
}

TEST_F(SqliteDatabaseTest, DropTable) {
  SetUp(GenerateDbPath());
  std::string table_name = "test_table";
  CreateTable(table_name);

  std::string err_msg;
  auto res = db_->dropTable(table_name, err_msg);
  EXPECT_TRUE(res);
  EXPECT_TRUE(err_msg.empty());

  std::string query_sql = "select name from sqlite_master where type='table';";
  err_msg.clear();
  std::vector<std::string> result;
  res = db_->query(query_sql, static_cast<void *>(&result),
                   fetch_strings_callback, err_msg);
  EXPECT_TRUE(res);
  EXPECT_TRUE(err_msg.empty());
  EXPECT_EQ(result.size(), 0);
}

}
} // namespace Database
} // namespace Libs
} // namespace v1_0_x
} // namespace TestSrhinoPluginFrameWork