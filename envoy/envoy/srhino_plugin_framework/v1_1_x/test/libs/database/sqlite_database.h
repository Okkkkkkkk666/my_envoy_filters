#pragma once

#include <gmock/gmock.h>
#include <srhino_plugin_framework/libs/database/sqlite_database.h>

namespace SrhinoPluginFramework {
namespace Test {
namespace Libs {
namespace Database {

using namespace SrhinoPluginFramework::Libs::Database;

class MockSqliteDatabase : public SrhinoPluginFramework::Libs::Database::SqliteDatabase {
public:
  MockSqliteDatabase() {}

public:
  MOCK_METHOD(bool, execute, (const std::string&, std::string&), ());
  MOCK_METHOD(bool, insert, (const std::string&, std::string&), ());
  MOCK_METHOD(bool, query, (const std::string&, void*, SqliteQueryCb, std::string&), ());
  MOCK_METHOD(bool, dropTable, (const std::string&, std::string&), ());
  MOCK_METHOD(const std::string&, db_name, (), ());
};

} // namespace Database
} // namespace Libs
} // namespace Test
} // namespace SrhinoPluginFramework