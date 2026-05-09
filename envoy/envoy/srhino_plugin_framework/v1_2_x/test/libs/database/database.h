#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_2_x/libs/database/database.h"
#include "envoy/srhino_plugin_framework/v1_2_x/test/libs/database/central_database.h"
#include "envoy/srhino_plugin_framework/v1_2_x/test/libs/database/sqlite_database.h"
#include "envoy/srhino_plugin_framework/v1_2_x/test/libs/database/ip_region_search.h"
namespace SrhinoPluginFramework {
namespace Test {
namespace Libs {
namespace Database {

using namespace SrhinoPluginFramework::Libs::Database;
using testing::_;
using testing::Return;

class MockDatabase : public SrhinoPluginFramework::Libs::Database::Database {
public:
  MockDatabase() {
    EXPECT_CALL(*this, createCentralDatabase(_, _))
        .WillRepeatedly(
            [](const std::chrono::milliseconds& timeout, const std::string& name_space) {
              return std::make_shared<MockCentralDatabase>(timeout, name_space);
            });
    EXPECT_CALL(*this, createSqliteDatabase(_)).WillRepeatedly([&](const std::string& db_path) {
      sqlite_db_ptr_ = std::shared_ptr<testing::NiceMock<MockSqliteDatabase>>(
          &sqlite_db_, [](testing::NiceMock<MockSqliteDatabase>* ptr) {});
      return sqlite_db_ptr_;
    });
    EXPECT_CALL(*this, createIpRegionSearch(_)).WillRepeatedly([&](const std::string& db_path) {
      ip_region_db_ptr_ = std::shared_ptr<testing::NiceMock<MockIpRegionSearch>>(
          &ip_region_db_, [](testing::NiceMock<MockIpRegionSearch>* ptr) {});
      return ip_region_db_ptr_;
    });
  }

public:
  MOCK_METHOD(CentralDatabaseSharedPtr, createCentralDatabase,
              (const std::chrono::milliseconds&, const std::string&), ());

  MOCK_METHOD(SqliteDatabaseSharedPtr, createSqliteDatabase, (const std::string&), ());

  MOCK_METHOD(IpRegionSearchSharedPtr, createIpRegionSearch, (const std::string&), ());

public:
  std::shared_ptr<testing::NiceMock<MockSqliteDatabase>> sqlite_db_ptr_;
  testing::NiceMock<MockSqliteDatabase> sqlite_db_;
  std::shared_ptr<testing::NiceMock<MockIpRegionSearch>> ip_region_db_ptr_;
  testing::NiceMock<MockIpRegionSearch> ip_region_db_;
};

} // namespace Database
} // namespace Libs
} // namespace Test
} // namespace SrhinoPluginFramework
