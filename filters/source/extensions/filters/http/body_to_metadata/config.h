#pragma once

#include "filters/api/envoy/extensions/filters/http/body_to_metadata/v3/body_to_metadata.pb.h"
#include "filters/api/envoy/extensions/filters/http/body_to_metadata/v3/body_to_metadata.pb.validate.h"

#include "source/extensions/filters/http/common/factory_base.h"

#include "filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace BodyToMetadataFilter {

class BodyToMetadataConfigFactory
    : public Common::FactoryBase<envoy::extensions::filters::http::body_to_metadata::v3::BodyToMetadataGlobal> {
public:
  BodyToMetadataConfigFactory() : FactoryBase(FILTER_NAME) {}

private:
  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::http::body_to_metadata::v3::BodyToMetadataGlobal& proto_config,
      const std::string& stats_prefix, Server::Configuration::FactoryContext& context) override;
};

DECLARE_FACTORY(BodyToMetadataConfigFactory);

} // namespace BodyToMetadataFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
