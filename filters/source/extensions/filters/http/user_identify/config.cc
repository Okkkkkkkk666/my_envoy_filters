#include <string>
#include "envoy/registry/registry.h"
#include "config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace UserIdentify {

Http::FilterFactoryCb UserIdentifyFilterFactory::createFilterFactoryFromProtoTyped(
    const v3::UserIdentifyGlobal& proto_config, const std::string&,
    Server::Configuration::FactoryContext& context) {
  std::string url_list = context.api().fileSystem().fileReadToEnd(PROTOBUF_GET_STRING_OR_DEFAULT(
      proto_config, url_filename, "plugins/user_identify/conf/url.txt"));
  std::string user_name_list =
      context.api().fileSystem().fileReadToEnd(PROTOBUF_GET_STRING_OR_DEFAULT(
          proto_config, user_filename, "plugins/user_identify/conf/username.txt"));
  std::string token_list = context.api().fileSystem().fileReadToEnd(PROTOBUF_GET_STRING_OR_DEFAULT(
      proto_config, token_filename, "plugins/user_identify/conf/token.txt"));
  std::string guest_list = context.api().fileSystem().fileReadToEnd(PROTOBUF_GET_STRING_OR_DEFAULT(
      proto_config, guest_filename, "plugins/user_identify/conf/guest.txt"));
  auto filter_config = std::make_shared<UserIdentifyFilterGlobalConfig>(
      proto_config, context, url_list, user_name_list, token_list, guest_list);
  const std::chrono::milliseconds timeout =
      std::chrono::milliseconds(PROTOBUF_GET_MS_OR_DEFAULT(proto_config.grpc(), timeout, 20));
  return [filter_config, proto_config, timeout,
          &context](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    auto client = std::make_unique<Filters::Common::CentralDatabase::Database>(
        context, proto_config.grpc(), timeout, FILTER_NAME);
    callbacks.addStreamFilter(std::make_shared<UserIdentifyFilter>(
        filter_config, std::move(client), context.getServerFactoryContext()));
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
UserIdentifyFilterFactory::createRouteSpecificFilterConfigTyped(
    const v3::UserIdentifyRoute& proto_config, Server::Configuration::ServerFactoryContext& context,
    ProtobufMessage::ValidationVisitor&) {
  return std::make_shared<UserIdentifyFilterRouteConfig>(proto_config, context,
                                                         context.mainThreadDispatcher());
}

REGISTER_FACTORY(UserIdentifyFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){"envoy.user_identify"};

} // namespace UserIdentify
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy