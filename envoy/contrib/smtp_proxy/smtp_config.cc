#include "contrib/smtp_proxy/smtp_config.h"
#include "contrib/smtp_proxy/smtp_filter.h"
#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace SmtpProxy {

Network::FilterFactoryCb SmtpFilterConfigFactory::createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::network::smtp_proxy::v3::SmtpProxy& proto_config,
      Server::Configuration::FactoryContext&){   
  SmtpConfigSharedPtr smtp_config = std::make_shared<SmtpConfig>(proto_config.stat_prefix(), proto_config.max_line_length());
  
  return [smtp_config](Network::FilterManager& filter_manager){
    filter_manager.addReadFilter(std::make_shared<SmtpFilter>(smtp_config));
  };
}

REGISTER_FACTORY(SmtpFilterConfigFactory, Server::Configuration::NamedNetworkFilterConfigFactory);
} // namespace SmtpProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy