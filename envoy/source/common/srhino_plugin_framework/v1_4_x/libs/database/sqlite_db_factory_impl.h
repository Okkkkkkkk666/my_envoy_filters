#pragma once

#include <memory>
#include <string>

#include "envoy/srhino_plugin_framework/v1_4_x/libs/database/sqlite_db_factory.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Database {

class SqliteDbFactoryImpl : public SqliteDbFactory {
public:
  SqliteDbFactoryImpl() {};
  virtual ~SqliteDbFactoryImpl() = default;

public:
  SqliteDatabaseSharedPtr createSqliteDatabase(const std::string& db_name) override;
};

} // namespace Database
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework