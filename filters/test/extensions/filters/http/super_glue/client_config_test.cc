#include <chrono>
#include "test/mocks/server/mocks.h"
#include "test/mocks/server/instance.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/mocks/api/mocks.h"
#include "test/test_common/utility.h"
#include "test/test_common/environment.h"
#include "filters/source/extensions/filters/http/super_glue/super_glue_client.h"
#include "filters/source/extensions/filters/http/super_glue/client_config.h"

using testing::NiceMock;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SuperGlueFilter {

namespace v3 = envoy::extensions::filters::http::super_glue::v3;

// 验证全局配置是否创建正确
TEST(Factory, GlobalConfig) {

  SuperGlueClientFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyConfigProto();
  NiceMock<Server::Configuration::MockFactoryContext> context_;
  auto callback = factory.createFilterFactoryFromProto(*proto_config, "stats", context_);
  Http::MockFilterChainFactoryCallbacks filter_callback;

  EXPECT_CALL(filter_callback, addStreamFilter(_));
  callback(filter_callback);
}

// 验证VH配置是否创建正确
TEST(Factory, RouteConfig) {
  const std::string yaml = R"(
    cluster_list: 
      cluster: acl
      unhealthy_bypass: false
      timeout_bypass: false
      request_timeout: 5s
    rules:
    - rule_name: "rule1"
      enable: true
      matchers:
      - prefix: /login
    - rule_name: "rule1"
      enable: false
      matchers:
      - prefix: /login
    percent:
      numerator: 100
      denominator: HUNDRED
  )";
  SuperGlueClientFilterFactory factory;
  ProtobufTypes::MessagePtr route_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *route_config);
  NiceMock<Server::Configuration::MockServerFactoryContext> context_;
  auto route = factory.createRouteSpecificFilterConfig(*route_config, context_,
                                                       ProtobufMessage::getNullValidationVisitor());

  const auto config = dynamic_cast<const ClientFilterRouteConfig*>(route.get());
  EXPECT_EQ(config->clusterList()[0]->requestTimeout(), std::chrono::milliseconds(5000));
  EXPECT_FALSE(config->getThreadLocalCluster("local_cluster"));
  EXPECT_FALSE(config->clusterList()[0]->timeoutBypass());
  EXPECT_FALSE(config->clusterList()[0]->unhealthyBypass());
  EXPECT_EQ(config->clusterList()[0]->clusterName(), "acl");
  EXPECT_EQ(config->rules()[0]->ruleName(), "rule1");
  EXPECT_EQ(config->rules().size(), 1);
}

} // namespace SuperGlueFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
