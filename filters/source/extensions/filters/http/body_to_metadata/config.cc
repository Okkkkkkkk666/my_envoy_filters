#include <chrono>
#include <cstdint>
#include <string>

#include "envoy/registry/registry.h"
#include "source/common/common/assert.h"

#include "config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace BodyToMetadataFilter {

Http::FilterFactoryCb BodyToMetadataConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::extensions::filters::http::body_to_metadata::v3::BodyToMetadataGlobal&
        proto_config,
    const std::string&, Server::Configuration::FactoryContext& context) {

  BodyToMetadataGlobalConfigSharedPtr filter_config(new BodyToMetadataGlobalConfig(proto_config));
  return [filter_config, &server_context = context.getServerFactoryContext()](
             Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<Filter>(filter_config, server_context));
  };
}

REGISTER_FACTORY(BodyToMetadataConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){"envoy.body_to_metadata"};

} // namespace AclFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
