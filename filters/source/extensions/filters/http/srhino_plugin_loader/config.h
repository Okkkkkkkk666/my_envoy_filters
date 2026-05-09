#pragma once

#include "source/extensions/filters/http/common/factory_base.h"

#include "filters/api/envoy/extensions/filters/http/srhino_plugin_loader/v3/srhino_plugin_loader.pb.h"
#include "filters/api/envoy/extensions/filters/http/srhino_plugin_loader/v3/srhino_plugin_loader.pb.validate.h"

#include "filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SrhinoPluginLoaderFilter {

class SrhinoPluginLoaderFilterFactory : public Common::FactoryBase<v3::SrhinoPluginLoaderGlobal, v3::SrhinoPluginLoaderPerRoute> {
public:
  SrhinoPluginLoaderFilterFactory() : FactoryBase(FILTER_NAME) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const v3::SrhinoPluginLoaderGlobal& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;
  Router::RouteSpecificFilterConfigConstSharedPtr createRouteSpecificFilterConfigTyped(
      const v3::SrhinoPluginLoaderPerRoute& proto_config,
      Server::Configuration::ServerFactoryContext&, ProtobufMessage::ValidationVisitor&) override;
};

DECLARE_FACTORY(SrhinoPluginLoaderFilterFactory);

} // namespace SrhinoPluginLoaderFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
