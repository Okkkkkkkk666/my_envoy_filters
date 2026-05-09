#pragma once

#include "source/extensions/filters/http/common/factory_base.h"
#include "filters/api/envoy/extensions/filters/http/super_glue/v3/super_glue_client.pb.h"
#include "filters/api/envoy/extensions/filters/http/super_glue/v3/super_glue_client.pb.validate.h"
#include "super_glue_client.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SuperGlueFilter {

class SuperGlueClientFilterFactory
    : public Common::FactoryBase<v3::SuperGlueClientGlobal, v3::SuperGlueClientRoute> {
public:
  SuperGlueClientFilterFactory() : FactoryBase(FILTER_NAME) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const v3::SuperGlueClientGlobal& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(const v3::SuperGlueClientRoute& proto_config,
                                       Server::Configuration::ServerFactoryContext& context,
                                       ProtobufMessage::ValidationVisitor&) override;
};

DECLARE_FACTORY(SuperGlueClientFilterFactory);

} // namespace SuperGlueFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
