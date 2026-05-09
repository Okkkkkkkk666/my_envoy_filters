#include <chrono>
#include <cstdint>
#include <string>

#include "envoy/registry/registry.h"
#include "source/common/common/assert.h"
#include "google/protobuf/util/json_util.h"

#include "config.h"
#include "sensitive_detect.h"

#include <google/protobuf/util/json_util.h>
#include <iostream>
using google::protobuf::util::JsonStringToMessage;

bool json_to_proto(const std::string& json, google::protobuf::Message& message) {
  return JsonStringToMessage(json, &message).ok();
}

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SensitiveDetect {

Http::FilterFactoryCb SensitiveDetectFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::extensions::filters::http::sensitive_detect::v3::SensitiveDetectGlobal&
        proto_config,
    const std::string&, Server::Configuration::FactoryContext& context) {
  // 解压缩gzip
  google::protobuf::Any config_gzip;
  std::string json_gzip = R"({
 "name": "gzip",
 "typed_config": {
  "@type": "type.googleapis.com/envoy.extensions.compression.gzip.decompressor.v3.Gzip"
 }
})";
  json_to_proto(json_gzip, config_gzip);
  std::string decompressor_library_gzip_type =
      "envoy.extensions.compression.gzip.decompressor.v3.Gzip";
  Decompressor::NamedDecompressorLibraryConfigFactory* const decompressor_library_gzip_factory =
      Registry::FactoryRegistry<Decompressor::NamedDecompressorLibraryConfigFactory>::
          getFactoryByType(decompressor_library_gzip_type);
  if (decompressor_library_gzip_factory == nullptr) {
    throw EnvoyException(fmt::format("Didn't find a registered implementation for type: '{}'",
                                     decompressor_library_gzip_type));
  }
  ProtobufTypes::MessagePtr message_decompressor_gzip =
      Config::Utility::translateAnyToFactoryConfig(config_gzip, context.messageValidationVisitor(),
                                                   *decompressor_library_gzip_factory);
  Decompressor::DecompressorFactoryPtr decompressor_gzip_factory =
      decompressor_library_gzip_factory->createDecompressorFactoryFromProto(
          *message_decompressor_gzip, context);

  // 解压缩brotli
  google::protobuf::Any config_br;
  std::string json_br = R"({
 "name": "Brotli",
 "typed_config": {
  "@type": "type.googleapis.com/envoy.extensions.compression.brotli.decompressor.v3.Brotli"
 }
})";
  json_to_proto(json_br, config_br);
  std::string decompressor_library_brotli_type =
      "envoy.extensions.compression.brotli.decompressor.v3.Brotli";
  Decompressor::NamedDecompressorLibraryConfigFactory* const decompressor_library_brotli_factory =
      Registry::FactoryRegistry<Decompressor::NamedDecompressorLibraryConfigFactory>::
          getFactoryByType(decompressor_library_brotli_type);
  if (decompressor_library_brotli_factory == nullptr) {
    throw EnvoyException(fmt::format("Didn't find a registered implementation for type: '{}'",
                                     decompressor_library_brotli_type));
  }
  ProtobufTypes::MessagePtr message_decompressor_brotli =
      Config::Utility::translateAnyToFactoryConfig(config_br, context.messageValidationVisitor(),
                                                   *decompressor_library_brotli_factory);
  Decompressor::DecompressorFactoryPtr decompressor_brotli_factory =
      decompressor_library_brotli_factory->createDecompressorFactoryFromProto(
          *message_decompressor_brotli, context);

  SensitiveDetectGlobalConfigSharedPtr filter_config(new SensitiveDetectGlobalConfig(
      proto_config, std::move(decompressor_gzip_factory), std::move(decompressor_brotli_factory)));
  return [filter_config, &server_context = context.getServerFactoryContext()](
             Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamEncoderFilter(
        std::make_shared<SensitiveDetect>(filter_config, server_context));
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
SensitiveDetectFilterConfigFactory::createRouteSpecificFilterConfigTyped(
    const envoy::extensions::filters::http::sensitive_detect::v3::SensitiveDetectPerRoute&
        proto_config,
    Server::Configuration::ServerFactoryContext&, ProtobufMessage::ValidationVisitor&) {
  return std::make_shared<const SensitiveDetectRouteConfig>(proto_config);
}

REGISTER_FACTORY(SensitiveDetectFilterConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory){"envoy.sensitive_detect"};

} // namespace SensitiveDetect
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy