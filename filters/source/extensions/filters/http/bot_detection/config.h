#pragma once

#include "source/extensions/filters/http/common/factory_base.h"

#include "filters/api/envoy/extensions/filters/http/bot_detection/v3/bot_detection.pb.h"
#include "filters/api/envoy/extensions/filters/http/bot_detection/v3/bot_detection.pb.validate.h"

#include "bot_detection.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace BotDetection {

class BotDetectionFilterFactory : public Common::FactoryBase<v3::BotDetectionGlobal, v3::BotDetectionRoute> {
public:
  BotDetectionFilterFactory() : FactoryBase(FILTER_NAME) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const v3::BotDetectionGlobal& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(const v3::BotDetectionRoute& proto_config,
                                       Server::Configuration::ServerFactoryContext& context,
                                       ProtobufMessage::ValidationVisitor& validator) override;
};

DECLARE_FACTORY(BotDetectionFilterFactory);
} // namespace BotDetection
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy