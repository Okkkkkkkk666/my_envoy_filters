#include "test/mocks/server/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filters/source/extensions/filters/http/weak_password_check/config.h"
#include "filters/source/extensions/filters/http/weak_password_check/weak_password_check.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WeakPasswordCheck {

namespace v3 = envoy::extensions::filters::http::weak_password_check::v3;

// 验证全局配置是否创建正确
TEST(Factory, GlobalConfig) {
  #if 1
  const std::string yaml = R"(
      auto_mode: false
      advance_config:
        - user_name: ["email", "username","21212121","account"]
          password_name: ["pswwd","crypt"]
          cluster_name: ["cluster2", "cluster3", "service_oa"]
      custom_password:
        rule_config:
          - part_rule: ["@", "dd", "123", "sr", "tiantian"]
          - part_rule: ["3", "david", "david", "zhongguo", "shanghai", "wo"]
        date_range: ["2014-01-11", "2024-02-21"]
        date_format: 7
        filter_instance_id: "weak-password.001"
    )";
  v3::WeakPasswordCheckGlobal global_config;
  TestUtility::loadFromYaml(yaml,global_config);
  WeakPasswordCheckFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);
  NiceMock<Server::Configuration::MockFactoryContext> context_;
  auto callback = factory.createFilterFactoryFromProto(*proto_config, "stat", context_);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamFilter(_));
  callback(filter_callback);

  auto filter_config = std::make_shared<FilterGlobalConfig>(global_config, context_);
  EXPECT_TRUE(filter_config->config());
  #endif
}

// 创建路由配置
TEST(Factory, RouteConfig) {
  WeakPasswordCheckFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, context, ProtobufMessage::getNullValidationVisitor());
}



} // namespace WeakPasswordCheck
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
