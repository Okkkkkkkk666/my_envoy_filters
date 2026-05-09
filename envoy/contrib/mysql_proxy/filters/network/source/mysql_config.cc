#include "contrib/mysql_proxy/filters/network/source/mysql_config.h"

#include <string>

#include "envoy/registry/registry.h"
#include "envoy/extensions/transport_sockets/tls/v3/secret.pb.h"
#include "envoy/secret/secret_manager.h"
#include "envoy/server/filter_config.h"

#include "source/common/common/logger.h"

#include "contrib/envoy/extensions/filters/network/mysql_proxy/v3/mysql_proxy.pb.h"
#include "contrib/envoy/extensions/filters/network/mysql_proxy/v3/mysql_proxy.pb.validate.h"
#include "contrib/mysql_proxy/filters/network/source/mysql_filter.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MySQLProxy {

namespace {
Secret::GenericSecretConfigProviderSharedPtr
secretProvider(const envoy::extensions::transport_sockets::tls::v3::SdsSecretConfig& config,
               Secret::SecretManager& secret_manager,
               Server::Configuration::TransportSocketFactoryContext& transport_socket_factory) {
  if (config.name().empty()) {
    return nullptr;
  }
  if (config.has_sds_config()) {
    return secret_manager.findOrCreateGenericSecretProvider(config.sds_config(), config.name(),
                                                            transport_socket_factory);
  }
  return secret_manager.findStaticGenericSecretProvider(config.name());
}
} // namespace

/**
 * Config registration for the MySQL proxy filter. @see NamedNetworkFilterConfigFactory.
 */
Network::FilterFactoryCb
NetworkFilters::MySQLProxy::MySQLConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::extensions::filters::network::mysql_proxy::v3::MySQLProxy& proto_config,
    Server::Configuration::FactoryContext& context) {

  ASSERT(!proto_config.stat_prefix().empty());

  const std::string stat_prefix = fmt::format("mysql.{}", proto_config.stat_prefix());
  std::vector<ProtectedColumnRule> protected_columns;
  protected_columns.reserve(proto_config.protected_columns_size());
  for (const auto& rule : proto_config.protected_columns()) {
    protected_columns.push_back(
        {rule.database(), rule.table(), rule.column()});
  }

  auto& cluster_manager = context.clusterManager();
  auto& secret_manager = cluster_manager.clusterManagerFactory().secretManager();
  auto& transport_socket_factory = context.getTransportSocketFactoryContext();
  auto fpe_secret_provider =
      secretProvider(proto_config.fpe_secret(), secret_manager, transport_socket_factory);

  MySQLFilterConfigSharedPtr filter_config(
      std::make_shared<MySQLFilterConfig>(stat_prefix, context.scope(), std::move(protected_columns),
                                          std::move(fpe_secret_provider), context.api()));
  return [filter_config](Network::FilterManager& filter_manager) -> void {
    filter_manager.addFilter(std::make_shared<MySQLFilter>(filter_config));
  };
}

/**
 * Static registration for the MySQL proxy filter. @see RegisterFactory.
 */
REGISTER_FACTORY(MySQLConfigFactory, Server::Configuration::NamedNetworkFilterConfigFactory);

} // namespace MySQLProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
