#pragma once 

#include <string>
#include <memory>
#include "source/extensions/filters/network/common/factory_base.h"
#include "contrib/envoy/extensions/filters/network/smtp_proxy/smtp_proxy.pb.h"
#include "contrib/envoy/extensions/filters/network/smtp_proxy/smtp_proxy.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace SmtpProxy {

class SmtpFilterConfigFactory
      : public Common::FactoryBase<envoy::extensions::filters::network::smtp_proxy::v3::SmtpProxy> {
public:
  SmtpFilterConfigFactory(): FactoryBase("envoy.filters.network.smtp_proxy") {}

private:
  Network::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::network::smtp_proxy::v3::SmtpProxy& proto_config,
      Server::Configuration::FactoryContext&) override;
};



} // namespace SmtpProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
