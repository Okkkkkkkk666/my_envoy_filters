#pragma once

#include "source/extensions/filters/http/common/factory_base.h"

#include "filters/api/envoy/extensions/filters/http/api_check/v3/api_check.pb.h"
#include "filters/api/envoy/extensions/filters/http/api_check/v3/api_check.pb.validate.h"

#include "api_check.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ApiCheckFilter {

class ApiCheckFilterFactory
    : public Common::FactoryBase<v3::ApiCheckGlobal,v3::ApiCheckRoute> {
public:
  ApiCheckFilterFactory() : FactoryBase(filter_name) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const v3::ApiCheckGlobal& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr createRouteSpecificFilterConfigTyped(
      const v3::ApiCheckRoute& proto_config,
      Server::Configuration::ServerFactoryContext& context,
      ProtobufMessage::ValidationVisitor& validator) override;

};

DECLARE_FACTORY(ApiCheckFilterFactory);

} // namespace ApiCheckFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy