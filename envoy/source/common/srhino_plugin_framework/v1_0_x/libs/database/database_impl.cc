#include "source/common/srhino_plugin_framework/v1_0_x/libs/database/database_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/database/central_database_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/database/sqlite_database_impl.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace Database {

DatabaseImpl::DatabaseImpl(Envoy::Server::Configuration::FactoryContext& factory_context)
    : factory_context_(factory_context) {}

CentralDatabaseSharedPtr
DatabaseImpl::createCentralDatabase(const std::chrono::milliseconds& timeout,
                                    const std::string& name_space) {
  return std::make_shared<CentralDatabaseImpl>(factory_context_, timeout, name_space);
}

SqliteDatabaseSharedPtr DatabaseImpl::createSqliteDatabase(const std::string& db_name) {
  return std::make_shared<SqliteDatabaseImpl>(db_name);
}

} // namespace Database
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework