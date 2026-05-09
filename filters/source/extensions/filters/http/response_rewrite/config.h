#pragma once

#include "filters/api/envoy/extensions/filters/http/response_rewrite/v3/response_rewrite.pb.h"
#include "filters/api/envoy/extensions/filters/http/response_rewrite/v3/response_rewrite.pb.validate.h"

#include "source/extensions/filters/http/common/factory_base.h"

#include "response_rewrite.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ResponseRewrite {

/**
 * Config registration for the responserewrite filter.
 */
class ResponseRewriteFilterConfigFactory
    : public Common::FactoryBase<
          envoy::extensions::filters::http::response_rewrite::v3::ResponseRewriteGlobal,
          envoy::extensions::filters::http::response_rewrite::v3::ResponseRewritePerRoute> {
public:
  ResponseRewriteFilterConfigFactory() : FactoryBase(FILTER_NAME) {}

private:
  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::http::response_rewrite::v3::ResponseRewriteGlobal&
          proto_config,
      const std::string& stats_prefix, Server::Configuration::FactoryContext& context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr createRouteSpecificFilterConfigTyped(
      const envoy::extensions::filters::http::response_rewrite::v3::ResponseRewritePerRoute&,
      Server::Configuration::ServerFactoryContext&, ProtobufMessage::ValidationVisitor&) override;
};

DECLARE_FACTORY(ResponseRewriteFilterConfigFactory);

} // namespace ResponseRewrite
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
