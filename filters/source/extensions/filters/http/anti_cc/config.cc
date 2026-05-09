#include "config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AntiCC {

Http::FilterFactoryCb AntiCCFilterFactory::createFilterFactoryFromProtoTyped(
    const v3::AntiCCGlobal& proto_config, const std::string&,
    Server::Configuration::FactoryContext& context) {
  FilterGlobalConfigSharedPtr filter_config =
      std::make_shared<AntiCCFilterGlobalConfig>(proto_config, context.getServerFactoryContext());
  const std::chrono::milliseconds timeout = std::chrono::milliseconds(
      PROTOBUF_GET_MS_OR_DEFAULT(proto_config.external_grpc_server(), timeout, 20));

  return [proto_config, filter_config, timeout,
          &context](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    auto blacklist_client = std::make_unique<Filters::Common::CentralDatabase::Database>(
        context, proto_config.external_grpc_server(), timeout, FILTER_NAME);
    auto verify_status_client = std::make_unique<Filters::Common::CentralDatabase::Database>(
        context, proto_config.external_grpc_server(), timeout, FILTER_NAME);
    callbacks.addStreamFilter(std::make_shared<AntiCCFilter>(
        context, filter_config, std::move(blacklist_client), std::move(verify_status_client),
        Filters::Common::RatelimitClient::Impl::rateLimitClient(
            context, proto_config.external_grpc_server(), timeout)));
  };
}

REGISTER_FACTORY(AntiCCFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){"envoy.anti_cc"};

} // namespace AntiCC
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy