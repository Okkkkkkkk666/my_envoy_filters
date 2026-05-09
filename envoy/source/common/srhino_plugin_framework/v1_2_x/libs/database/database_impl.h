#pragma once

#include "envoy/server/factory_context.h"
#include "envoy/srhino_plugin_framework/v1_2_x/libs/database/database.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace Database {

class DatabaseImpl : public Database {
public:
  DatabaseImpl(Envoy::Server::Configuration::FactoryContext& factory_context);
  virtual ~DatabaseImpl() = default;

public:
  CentralDatabaseSharedPtr createCentralDatabase(const std::chrono::milliseconds& timeout,
                                                 const std::string& name_space) override;

  SqliteDatabaseSharedPtr createSqliteDatabase(const std::string& db_name) override;
  
  IpRegionSearchSharedPtr createIpRegionSearch(const std::string& xdb_name) override;

private:
  Envoy::Server::Configuration::FactoryContext& factory_context_;
};

} // namespace Database
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework