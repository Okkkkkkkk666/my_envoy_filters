#pragma once

#include "source/extensions/filters/http/common/factory_base.h"

#include "filters/api/envoy/extensions/filters/http/strong_local_ratelimit/v3/strong_local_ratelimit.pb.h"
#include "filters/api/envoy/extensions/filters/http/strong_local_ratelimit/v3/strong_local_ratelimit.pb.validate.h"

#include "strong_local_ratelimit.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {

/**
 * Config registration for the another local rate limit filter.
 */
class StrongLocalRateLimitFilterFactory
    : public Common::FactoryBase<v3::StrongLocalRateLimitGlobal, v3::StrongLocalRateLimitRoute> {
public:
  StrongLocalRateLimitFilterFactory() : FactoryBase(FILTER_NAME) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const v3::StrongLocalRateLimitGlobal& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr createRouteSpecificFilterConfigTyped(
      const envoy::extensions::filters::http::strong_local_ratelimit::v3::StrongLocalRateLimitRoute&
          proto_config,
      Server::Configuration::ServerFactoryContext& context,
      ProtobufMessage::ValidationVisitor&) override;
};

DECLARE_FACTORY(StrongLocalRateLimitFilterFactory);

} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
