#pragma once

#include "filters/api/envoy/extensions/filters/http/sensitive_detect/v3/sensitive_detect.pb.h"
#include "filters/api/envoy/extensions/filters/http/sensitive_detect/v3/sensitive_detect.pb.validate.h"

#include "source/extensions/filters/http/common/factory_base.h"

#include "sensitive_detect.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SensitiveDetect {

/**
 * Config registration for the SensitiveDetect filter.
 */
class SensitiveDetectFilterConfigFactory
    : public Common::FactoryBase<
          envoy::extensions::filters::http::sensitive_detect::v3::SensitiveDetectGlobal,
          envoy::extensions::filters::http::sensitive_detect::v3::SensitiveDetectPerRoute> {
public:
  SensitiveDetectFilterConfigFactory() : FactoryBase(FILTER_NAME) {}

private:
  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::http::sensitive_detect::v3::SensitiveDetectGlobal&
          proto_config,
      const std::string& stats_prefix, Server::Configuration::FactoryContext& context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr createRouteSpecificFilterConfigTyped(
      const envoy::extensions::filters::http::sensitive_detect::v3::SensitiveDetectPerRoute&,
      Server::Configuration::ServerFactoryContext&, ProtobufMessage::ValidationVisitor&) override;
};

DECLARE_FACTORY(SensitiveDetectFilterConfigFactory);

} // namespace SensitiveDetect
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
