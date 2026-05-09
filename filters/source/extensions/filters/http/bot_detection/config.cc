#include "config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace BotDetection {

Http::FilterFactoryCb BotDetectionFilterFactory::createFilterFactoryFromProtoTyped(
    const v3::BotDetectionGlobal& proto_config, const std::string&,
    Server::Configuration::FactoryContext& context) {
  FilterGlobalConfigSharedPtr filter_config =
      std::make_shared<BotDetectionFilterGlobalConfig>(proto_config);
  return [filter_config, &server_context = context.getServerFactoryContext()](
             Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<BotDetectionFilter>(filter_config, server_context));
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
BotDetectionFilterFactory::createRouteSpecificFilterConfigTyped(
    const v3::BotDetectionRoute& proto_config, Server::Configuration::ServerFactoryContext& context,
    ProtobufMessage::ValidationVisitor&) {
  std::string bot_list = context.api().fileSystem().fileReadToEnd(
      PROTOBUF_GET_STRING_OR_DEFAULT(proto_config, filename, "filters/source/extensions/filters/http/bot_detection/impl/bot_regex_list.txt"));
  return std::make_shared<BotDetectionFilterRouteConfig>(proto_config, bot_list);
}

REGISTER_FACTORY(BotDetectionFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){"envoy.bot_detection"};
} // namespace BotDetection
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy