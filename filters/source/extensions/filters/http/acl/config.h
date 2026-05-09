#pragma once

#include "filters/api/envoy/extensions/filters/http/acl/v3/acl.pb.h"
#include "filters/api/envoy/extensions/filters/http/acl/v3/acl.pb.validate.h"

#include "source/extensions/filters/http/common/factory_base.h"

#include "acl_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AclFilter {

/**
 * Config registration for the acl filter.
 */
class AclFilterConfigFactory
    : public Common::FactoryBase<envoy::extensions::filters::http::acl::v3::Acl,
                                 envoy::extensions::filters::http::acl::v3::AclPerRoute> {
public:
  AclFilterConfigFactory() : FactoryBase(FILTER_NAME) {}

private:
  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::http::acl::v3::Acl& proto_config,
      const std::string& stats_prefix, Server::Configuration::FactoryContext& context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr createRouteSpecificFilterConfigTyped(
      const envoy::extensions::filters::http::acl::v3::AclPerRoute&,
      Server::Configuration::ServerFactoryContext&, ProtobufMessage::ValidationVisitor&) override;
};

DECLARE_FACTORY(AclFilterConfigFactory);

} // namespace AclFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
