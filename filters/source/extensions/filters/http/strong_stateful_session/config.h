#pragma once

#include "filter.h"
#include "filters/api/envoy/extensions/filters/http/strong_stateful_session/v3/strong_stateful_session.pb.validate.h"
#include "source/extensions/filters/http/common/factory_base.h"

#include "filters/api/envoy/extensions/filters/http/strong_stateful_session/v3/strong_stateful_session.pb.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {

class StrongStatefulSessionFilterFactory
    : public Common::FactoryBase<v3::StrongStatefulSessionGlobal,
                                 v3::StrongStatefulSessionPerRoute> {
public:
  StrongStatefulSessionFilterFactory() : FactoryBase(FILTER_NAME) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const v3::StrongStatefulSessionGlobal& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr createRouteSpecificFilterConfigTyped(
      const v3::StrongStatefulSessionPerRoute& proto_config,
      Server::Configuration::ServerFactoryContext&, ProtobufMessage::ValidationVisitor&) override;
};

DECLARE_FACTORY(StrongStatefulSessionFilterFactory);

} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
