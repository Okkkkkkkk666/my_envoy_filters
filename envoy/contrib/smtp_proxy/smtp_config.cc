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
  
  std::string stat_prefix = proto_config.stat_prefix().empty() ? "default_smtp" : proto_config.stat_prefix();
  uint32_t max_line_length = proto_config.max_line_length() > 0 ? proto_config.max_line_length() : 1024;

  bool mime_enabled = false;
  uint32_t max_body_bytes = 0;
  if (proto_config.has_mime_config()) {
    mime_enabled = proto_config.mime_config().enable_mime_parsing();
    max_body_bytes = proto_config.mime_config().max_body_bytes() > 0 
                     ? proto_config.mime_config().max_body_bytes() : 10485760;
  }

  std::vector<std::string> denied_senders;
  if (proto_config.has_policy()) {
    for (const auto& sender : proto_config.policy().denied_senders()) {
      if (!sender.empty()) {
        denied_senders.push_back(sender);
      }
    }
  }

  SmtpConfigSharedPtr smtp_config = std::make_shared<SmtpConfig>(
      stat_prefix, max_line_length, mime_enabled, max_body_bytes, denied_senders);
  
  return [smtp_config](Network::FilterManager& filter_manager){
    filter_manager.addReadFilter(std::make_shared<SmtpFilter>(smtp_config));
  };
}

REGISTER_FACTORY(SmtpFilterConfigFactory, Server::Configuration::NamedNetworkFilterConfigFactory);

} // namespace SmtpProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy