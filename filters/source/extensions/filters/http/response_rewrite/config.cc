#include <chrono>
#include <cstdint>
#include <string>

#include "envoy/registry/registry.h"
#include "source/common/common/assert.h"

#include "config.h"
#include "response_rewrite.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ResponseRewrite {

Http::FilterFactoryCb ResponseRewriteFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::extensions::filters::http::response_rewrite::v3::ResponseRewriteGlobal&
        proto_config,
    const std::string&, Server::Configuration::FactoryContext& context) {

  // 解压缩gzip
  std::string decompressor_library_gzip_type{TypeUtil::typeUrlToDescriptorFullName(
      proto_config.decompressor_library_gzip().typed_config().type_url())};
  Decompressor::NamedDecompressorLibraryConfigFactory* const decompressor_library_gzip_factory =
      Registry::FactoryRegistry<Decompressor::NamedDecompressorLibraryConfigFactory>::
          getFactoryByType(decompressor_library_gzip_type);
  if (decompressor_library_gzip_factory == nullptr) {
    throw EnvoyException(fmt::format("Didn't find a registered implementation for type: '{}'",
                                     decompressor_library_gzip_type));
  }
  ProtobufTypes::MessagePtr message_decompressor_gzip =
      Config::Utility::translateAnyToFactoryConfig(
          proto_config.decompressor_library_gzip().typed_config(),
          context.messageValidationVisitor(), *decompressor_library_gzip_factory);
  Decompressor::DecompressorFactoryPtr decompressor_gzip_factory =
      decompressor_library_gzip_factory->createDecompressorFactoryFromProto(
          *message_decompressor_gzip, context);
  // 解压缩brotli
  std::string decompressor_library_brotli_type{TypeUtil::typeUrlToDescriptorFullName(
      proto_config.decompressor_library_brotli().typed_config().type_url())};
  Decompressor::NamedDecompressorLibraryConfigFactory* const decompressor_library_brotli_factory =
      Registry::FactoryRegistry<Decompressor::NamedDecompressorLibraryConfigFactory>::
          getFactoryByType(decompressor_library_brotli_type);
  if (decompressor_library_brotli_factory == nullptr) {
    throw EnvoyException(fmt::format("Didn't find a registered implementation for type: '{}'",
                                     decompressor_library_brotli_type));
  }
  ProtobufTypes::MessagePtr message_decompressor_brotli =
      Config::Utility::translateAnyToFactoryConfig(
          proto_config.decompressor_library_brotli().typed_config(),
          context.messageValidationVisitor(), *decompressor_library_brotli_factory);
  Decompressor::DecompressorFactoryPtr decompressor_brotli_factory =
      decompressor_library_brotli_factory->createDecompressorFactoryFromProto(
          *message_decompressor_brotli, context);

  // 压缩gzip
  std::string compressor_library_gzip_type{TypeUtil::typeUrlToDescriptorFullName(
      proto_config.compressor_library_gzip().typed_config().type_url())};
  Compressor::NamedCompressorLibraryConfigFactory* const compressor_library_gzip_factory =
      Registry::FactoryRegistry<Compressor::NamedCompressorLibraryConfigFactory>::getFactoryByType(
          compressor_library_gzip_type);
  if (compressor_library_gzip_factory == nullptr) {
    throw EnvoyException(fmt::format("Didn't find a registered implementation for type: '{}'",
                                     compressor_library_gzip_type));
  }
  ProtobufTypes::MessagePtr message_compressor_gzip = Config::Utility::translateAnyToFactoryConfig(
      proto_config.compressor_library_gzip().typed_config(), context.messageValidationVisitor(),
      *compressor_library_gzip_factory);
  Compressor::CompressorFactoryPtr compressor_gzip_factory =
      compressor_library_gzip_factory->createCompressorFactoryFromProto(*message_compressor_gzip,
                                                                        context);
  // 压缩brotli
  std::string compressor_library_brotli_type{TypeUtil::typeUrlToDescriptorFullName(
      proto_config.compressor_library_brotli().typed_config().type_url())};
  Compressor::NamedCompressorLibraryConfigFactory* const compressor_library_brotli_factory =
      Registry::FactoryRegistry<Compressor::NamedCompressorLibraryConfigFactory>::getFactoryByType(
          compressor_library_brotli_type);
  if (compressor_library_brotli_factory == nullptr) {
    throw EnvoyException(fmt::format("Didn't find a registered implementation for type: '{}'",
                                     compressor_library_brotli_type));
  }
  ProtobufTypes::MessagePtr message_compressor_brotli =
      Config::Utility::translateAnyToFactoryConfig(
          proto_config.compressor_library_brotli().typed_config(),
          context.messageValidationVisitor(), *compressor_library_brotli_factory);
  Compressor::CompressorFactoryPtr compressor_brotli_factory =
      compressor_library_brotli_factory->createCompressorFactoryFromProto(
          *message_compressor_brotli, context);

  ResponseRewriteGlobalConfigSharedPtr filter_config(new ResponseRewriteGlobalConfig(
      proto_config, std::move(decompressor_gzip_factory), std::move(decompressor_brotli_factory),
      std::move(compressor_gzip_factory), std::move(compressor_brotli_factory)));
  return [filter_config, &server_context = context.getServerFactoryContext()](
             Http::FilterChainFactoryCallbacks& callbacks) -> void {
        callbacks.addStreamFilter(
        std::make_shared<ResponseRewrite>(filter_config, server_context));
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
ResponseRewriteFilterConfigFactory::createRouteSpecificFilterConfigTyped(
    const envoy::extensions::filters::http::response_rewrite::v3::ResponseRewritePerRoute&
        proto_config,
    Server::Configuration::ServerFactoryContext&, ProtobufMessage::ValidationVisitor&) {
  // 没有必填字段，所以这里什么校验都没
  return std::make_shared<const ResponseRewriteRouteConfig>(proto_config);
}

/**
 * Static registration for the responserewrite filter. @see RegisterFactory.
 * 这样注册会有两个key在工厂里面，
 * 可以通过打印Registry::FactoryRegistry<Server::Configuration::NamedHttpFilterConfigFactory>::factories()
 * 来验证
 */
REGISTER_FACTORY(ResponseRewriteFilterConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){"envoy.responserewrite"};

} // namespace ResponseRewrite
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
