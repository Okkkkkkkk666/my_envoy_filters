#pragma once

#include "source/extensions/filters/http/common/factory_base.h"

#include "filters/api/envoy/extensions/filters/http/super_glue/v3/super_glue_server.pb.h"
#include "filters/api/envoy/extensions/filters/http/super_glue/v3/super_glue_server.pb.validate.h"

#include "super_glue_server.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SuperGlueFilter {

class SuperGlueServerFilterFactory
    : public Common::FactoryBase<v3::SuperGlueServerGlobal, v3::SuperGlueServerRoute> {
public:
  SuperGlueServerFilterFactory() : FactoryBase(FILTER_NAME) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const v3::SuperGlueServerGlobal& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(const v3::SuperGlueServerRoute& proto_config,
                                       Server::Configuration::ServerFactoryContext& context,
                                       ProtobufMessage::ValidationVisitor&) override;
};

DECLARE_FACTORY(SuperGlueServerFilterFactory);

} // namespace SuperGlueFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
