#pragma once

#include "source/extensions/filters/http/common/factory_base.h"

#include "filters/api/envoy/extensions/filters/http/waf/v3/waf.pb.h"
#include "filters/api/envoy/extensions/filters/http/waf/v3/waf.pb.validate.h"

#include "waf.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WafFilter {

class WafFilterFactory
    : public Common::FactoryBase<v3::WafGlobal,v3::WafRoute> {
public:
  WafFilterFactory() : FactoryBase(filter_name) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const v3::WafGlobal& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr createRouteSpecificFilterConfigTyped(
      const v3::WafRoute& proto_config,
      Server::Configuration::ServerFactoryContext& context,
      ProtobufMessage::ValidationVisitor& validator) override;

};

DECLARE_FACTORY(ModSecurityFilterFactory);

} // namespace ModSecurity
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy