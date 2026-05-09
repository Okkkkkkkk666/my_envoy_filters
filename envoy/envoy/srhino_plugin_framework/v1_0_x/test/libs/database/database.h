#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_0_x/libs/database/database.h"
#include "envoy/srhino_plugin_framework/v1_0_x/test/libs/database/central_database.h"
#include "envoy/srhino_plugin_framework/v1_0_x/test/libs/database/sqlite_database.h"

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
    EXPECT_CALL(*this, createSqliteDatabase(_))
        .WillRepeatedly(
          [&](const std::string& db_path) {
            sqlite_db_ptr_ = std::shared_ptr<testing::NiceMock<MockSqliteDatabase>>(&sqlite_db_,
                                 [](testing::NiceMock<MockSqliteDatabase> *ptr){});
            return sqlite_db_ptr_;
          }
        );
  }

public:
  MOCK_METHOD(CentralDatabaseSharedPtr, createCentralDatabase,
              (const std::chrono::milliseconds&, const std::string&), ());

  MOCK_METHOD(SqliteDatabaseSharedPtr, createSqliteDatabase, (const std::string&), ());

public:
  std::shared_ptr<testing::NiceMock<MockSqliteDatabase>> sqlite_db_ptr_;
  testing::NiceMock<MockSqliteDatabase> sqlite_db_;
};

} // namespace Database
} // namespace Libs
} // namespace Test
} // namespace SrhinoPluginFramework
