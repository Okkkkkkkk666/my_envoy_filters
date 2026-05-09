#include <chrono>
#include <cstdint>
#include <string>

#include "envoy/registry/registry.h"
#include "source/common/common/assert.h"

#include "config.h"
#include "acl_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AclFilter {

Http::FilterFactoryCb AclFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::extensions::filters::http::acl::v3::Acl& proto_config, const std::string&,
    Server::Configuration::FactoryContext& context) {

  AclFilterGlobalConfigSharedPtr filter_config(new AclFilterGlobalConfig(proto_config));
  return [filter_config, &server_context = context.getServerFactoryContext()](
             Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamDecoderFilter(std::make_shared<AclFilter>(filter_config, server_context));
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
AclFilterConfigFactory::createRouteSpecificFilterConfigTyped(
    const envoy::extensions::filters::http::acl::v3::AclPerRoute& proto_config,
    Server::Configuration::ServerFactoryContext&, ProtobufMessage::ValidationVisitor&) {
  //没有必填字段，所以这里什么校验都没
  return std::make_shared<const AclFilterRouteConfig>(proto_config);
}

/**
 * Static registration for the acl filter. @see RegisterFactory.
 * 这样注册会有两个key在工厂里面，
 * 可以通过打印Registry::FactoryRegistry<Server::Configuration::NamedHttpFilterConfigFactory>::factories()
 * 来验证
 */
REGISTER_FACTORY(AclFilterConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){"envoy.acl"};

} // namespace AclFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
