#include "test/mocks/server/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "source/extensions/compression/gzip/decompressor/config.h"
#include "source/extensions/compression/brotli/decompressor/config.h"
#include "envoy/event/timer.h"
#include "source/extensions/compression/gzip/compressor/config.h"
#include "source/extensions/compression/brotli/compressor/config.h"
#include "test/integration/http_integration.h"
#include "test/mocks/server/factory_context.h"
#include "test/test_common/simulated_time_system.h"
#include "test/test_common/utility.h"
#include "envoy/extensions/compression/gzip/compressor/v3/gzip.pb.h"
#include "envoy/extensions/compression/gzip/decompressor/v3/gzip.pb.h"

#include "test/integration/base_integration_test.h"
#include "test/mocks/compression/decompressor/mocks.h"
#include "test/mocks/compression/compressor/mocks.h"

#include "filters/source/extensions/filters/http/response_rewrite/config.h"
#include "filters/source/extensions/filters/http/response_rewrite/response_rewrite.h"
#include "filters/api/envoy/extensions/filters/http/response_rewrite/v3/response_rewrite.pb.h"
#include "filters/api/envoy/extensions/filters/http/response_rewrite/v3/response_rewrite_log.pb.h"
// #include "test/extensions/filters/http/compressor/mock_compressor_library.pb.h"
#include "filters/test/extensions/filters/http/response_rewrite/mock_compressor_library.pb.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ResponseRewrite {
using testing::NiceMock;
const ::test::mock_compressor_library::Unregistered _mock_compressor_library_dummy;
// 验证全局配置
TEST(Factory, GlobalConfig) {
  const std::string yaml = R"(
    decompressor_library_gzip:
      name: gzip
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.gzip.decompressor.v3.Gzip
    decompressor_library_brotli:
      name: br
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.brotli.decompressor.v3.Brotli
    compressor_library_gzip:
      name: gzip1
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip
    compressor_library_brotli:
      name: br1
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.brotli.compressor.v3.Brotli
  )";
  ResponseRewriteFilterConfigFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);
  NiceMock<Server::Configuration::MockFactoryContext> context;
  Http::FilterFactoryCb callback =
      factory.createFilterFactoryFromProto(*proto_config, "stats", context);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamEncoderFilter(_));
  callback(filter_callback);
}

TEST(Factory, DecompressorLibraryGzipFalse) {
  const std::string yaml = R"(
    decompressor_library_gzip:
      name: gzip
      typed_config:
        "@type": type.googleapis.com/test.mock_compressor_library.Unregistered
    decompressor_library_brotli:
      name: br
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.brotli.decompressor.v3.Brotli
    compressor_library_gzip:
      name: gzip1
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip
    compressor_library_brotli:
      name: br1
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.brotli.compressor.v3.Brotli
  )";
  envoy::extensions::filters::http::response_rewrite::v3::ResponseRewriteGlobal proto_config;
  TestUtility::loadFromYaml(yaml, proto_config);
  ResponseRewriteFilterConfigFactory factory;
  NiceMock<Server::Configuration::MockFactoryContext> context;
  EXPECT_THROW_WITH_MESSAGE(factory.createFilterFactoryFromProto(proto_config, "stats", context),
                            EnvoyException,
                            "Didn't find a registered implementation for type: "
                            "'test.mock_compressor_library.Unregistered'");
}

TEST(Factory, DecompressorLibraryBrotliFalse) {
  const std::string yaml = R"(
    decompressor_library_gzip:
      name: gzip
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.gzip.decompressor.v3.Gzip
    decompressor_library_brotli:
      name: br
      typed_config:
        "@type": type.googleapis.com/test.mock_compressor_library.Unregistered
    compressor_library_gzip:
      name: gzip1
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip
    compressor_library_brotli:
      name: br1
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.brotli.compressor.v3.Brotli
  )";
  envoy::extensions::filters::http::response_rewrite::v3::ResponseRewriteGlobal proto_config;
  TestUtility::loadFromYaml(yaml, proto_config);
  ResponseRewriteFilterConfigFactory factory;
  NiceMock<Server::Configuration::MockFactoryContext> context;
  EXPECT_THROW_WITH_MESSAGE(factory.createFilterFactoryFromProto(proto_config, "stats", context),
                            EnvoyException,
                            "Didn't find a registered implementation for type: "
                            "'test.mock_compressor_library.Unregistered'");
}

TEST(Factory, CompressorLibraryGzipFalse) {
  const std::string yaml = R"(
    decompressor_library_gzip:
      name: gzip
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.gzip.decompressor.v3.Gzip
    decompressor_library_brotli:
      name: br
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.brotli.decompressor.v3.Brotli
    compressor_library_gzip:
      name: gzip1
      typed_config:
        "@type": type.googleapis.com/test.mock_compressor_library.Unregistered
    compressor_library_brotli:
      name: br1
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.brotli.compressor.v3.Brotli
  )";
  envoy::extensions::filters::http::response_rewrite::v3::ResponseRewriteGlobal proto_config;
  TestUtility::loadFromYaml(yaml, proto_config);
  ResponseRewriteFilterConfigFactory factory;
  NiceMock<Server::Configuration::MockFactoryContext> context;
  EXPECT_THROW_WITH_MESSAGE(factory.createFilterFactoryFromProto(proto_config, "stats", context),
                            EnvoyException,
                            "Didn't find a registered implementation for type: "
                            "'test.mock_compressor_library.Unregistered'");
}

TEST(Factory, CompressorLibraryBrotliFalse) {
  const std::string yaml = R"(
    decompressor_library_gzip:
      name: gzip
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.gzip.decompressor.v3.Gzip
    decompressor_library_brotli:
      name: br
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.brotli.decompressor.v3.Brotli
    compressor_library_gzip:
      name: gzip1
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip
    compressor_library_brotli:
      name: br1
      typed_config:
        "@type": type.googleapis.com/test.mock_compressor_library.Unregistered
  )";
  envoy::extensions::filters::http::response_rewrite::v3::ResponseRewriteGlobal proto_config;
  TestUtility::loadFromYaml(yaml, proto_config);
  ResponseRewriteFilterConfigFactory factory;
  NiceMock<Server::Configuration::MockFactoryContext> context;
  EXPECT_THROW_WITH_MESSAGE(factory.createFilterFactoryFromProto(proto_config, "stats", context),
                            EnvoyException,
                            "Didn't find a registered implementation for type: "
                            "'test.mock_compressor_library.Unregistered'");
}

// 验证路由配置是否正确
TEST(Factory, RouteSpecificFilterConfig) {
  const std::string yaml = R"(
    enable: true
    status_codes:
      - 200
      - 404
    compressor_enable: true
  )";

  ResponseRewriteFilterConfigFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);
  NiceMock<Server::Configuration::MockServerFactoryContext> context;

  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, context, ProtobufMessage::getNullValidationVisitor());
  const auto* config = dynamic_cast<const ResponseRewriteRouteConfig*>(route_config.get());
  EXPECT_TRUE(config->enable());
  EXPECT_TRUE(config->compressor_enable());
  EXPECT_TRUE(config->matchStatusCode(200));
  EXPECT_TRUE(config->matchStatusCode(404));
  EXPECT_FALSE(config->matchStatusCode(500));
  auto statuscode = config->status_codes();
  EXPECT_EQ(statuscode.size(), 2);
  EXPECT_TRUE(statuscode.find(200) != statuscode.end());
  EXPECT_TRUE(statuscode.find(404) != statuscode.end());
}

TEST(Factory, AllByDefault) {
  const std::string yaml = R"(
    enable: false
  )";

  ResponseRewriteFilterConfigFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);

  NiceMock<Server::Configuration::MockServerFactoryContext> server_context;
  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, server_context, ProtobufMessage::getNullValidationVisitor());
  const auto* config = dynamic_cast<const ResponseRewriteRouteConfig*>(route_config.get());
  EXPECT_FALSE(config->enable());
  EXPECT_TRUE(config->status_codes().empty());
}

} // namespace ResponseRewrite
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
