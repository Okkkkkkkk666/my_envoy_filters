#include <chrono>
#include "test/mocks/server/mocks.h"
#include "test/mocks/server/instance.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/mocks/api/mocks.h"
#include "test/test_common/utility.h"
#include "test/test_common/environment.h"
#include "filters/source/extensions/filters/http/super_glue/super_glue_server.h"
#include "filters/source/extensions/filters/http/super_glue/server_config.h"

using testing::NiceMock;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SuperGlueFilter {

namespace v3 = envoy::extensions::filters::http::super_glue::v3;

// 验证全局配置是否创建正确
TEST(Factory, GlobalConfig) {
  
  SuperGlueServerFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyConfigProto();
  NiceMock<Server::Configuration::MockFactoryContext> context_;
  auto callback = factory.createFilterFactoryFromProto(*proto_config, "stats", context_);
  Http::MockFilterChainFactoryCallbacks filter_callback;

  EXPECT_CALL(filter_callback, addStreamFilter(_));
  callback(filter_callback);

}

// 创建路由配置
TEST(Factory, RouteConfig) {
  SuperGlueServerFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, context, ProtobufMessage::getNullValidationVisitor());
}
} // namespace SuperGlueFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy