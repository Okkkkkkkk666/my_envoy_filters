#include "test/mocks/server/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <iostream>

#include "source/extensions/compression/gzip/decompressor/config.h"
#include "source/extensions/compression/brotli/decompressor/config.h"
#include "filters/source/extensions/filters/http/sensitive_detect/config.h"
#include "filters/source/extensions/filters/http/sensitive_detect/sensitive_detect.h"
#include "envoy/extensions/compression/gzip/decompressor/v3/gzip.pb.h"
#include "test/integration/http_integration.h"
#include "test/mocks/server/factory_context.h"
#include "test/test_common/simulated_time_system.h"
#include "test/test_common/utility.h"
#include "test/integration/base_integration_test.h"
#include "test/mocks/compression/decompressor/mocks.h"
#include "test/mocks/compression/compressor/mocks.h"
#include "source/extensions/compression/gzip/decompressor/config.h"
#include "filters/test/extensions/filters/http/sensitive_detect/mock_compressor_library.pb.h"
namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SensitiveDetect {

namespace v3 = envoy::extensions::filters::http::sensitive_detect::v3;

// 验证全局配置
TEST(SensitiveConfigTest, GlobalConfig) {
  const std::string yaml = R"(
    enable: true
    rules:
      - id: 1
      - id: 2
      - id: 3
  )";
  SensitiveDetectFilterConfigFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);
  NiceMock<Server::Configuration::MockFactoryContext> context;
  Http::FilterFactoryCb callback =
      factory.createFilterFactoryFromProto(*proto_config, "stats", context);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamEncoderFilter(_));
  callback(filter_callback);
}

TEST(Factory, EmptyRouteConfig) {
  SensitiveDetectFilterConfigFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  const auto route_config = factory.createRouteSpecificFilterConfig(
    *proto_config, context, ProtobufMessage::getNullValidationVisitor());
}

} // namespace SensitiveDetect
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy