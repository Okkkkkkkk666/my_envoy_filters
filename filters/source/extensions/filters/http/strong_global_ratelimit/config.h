#pragma once

#include "source/extensions/filters/http/common/factory_base.h"

#include "filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3/strong_global_ratelimit.pb.h"
#include "filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3/strong_global_ratelimit.pb.validate.h"

#include "strong_global_ratelimit.h"
#include "filters/source/extensions/filters/http/common/ratelimit/impl/ratelimit_client_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongGlobalRatelimit {
class StrongGlobalRatelimitFilterFactory
    : public Common::FactoryBase<v3::StrongGlobalRateLimitGlobal, v3::StrongGlobalRateLimitRoute> {
public:
  StrongGlobalRatelimitFilterFactory() : FactoryBase(FILTER_NAME) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const v3::StrongGlobalRateLimitGlobal& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;
  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(const v3::StrongGlobalRateLimitRoute& proto_config,
                                       Server::Configuration::ServerFactoryContext& context,
                                       ProtobufMessage::ValidationVisitor& validator) override;
};
DECLARE_FACTORY(StrongGlobalRatelimitFilterFactory);
} // namespace StrongGlobalRatelimit
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
