#include "source/common/srhino_plugin_framework/v1_1_x/libs/database/sqlite_db_factory_impl.h"
#include "source/common/srhino_plugin_framework/v1_1_x/libs/database/sqlite_database_impl.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Database {

SqliteDatabaseSharedPtr SqliteDbFactoryImpl::createSqliteDatabase(const std::string& db_name) {
  return std::make_shared<SqliteDatabaseImpl>(db_name);
}

} // namespace Database
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework