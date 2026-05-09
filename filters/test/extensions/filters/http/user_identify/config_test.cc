#include "test/mocks/server/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filters/source/extensions/filters/http/user_identify/config.h"
#include "filters/source/extensions/filters/http/user_identify/user_identify.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace UserIdentify {

namespace v3 = envoy::extensions::filters::http::user_identify::v3;

// 验证全局配置是否创建正确
TEST(Factory, GlobalConfig) {
  const std::string yaml = R"(
        customize:
          customization:
            cluster_name: service_oa,nihaoya,hahaha
            url:
              ["192.192.101.50:7777/api/login"]
            user_name: ["email", "username"]
        url_filename: user_identify/impl/url.txt
        user_filename: user_identify/impl/username.txt
        token_filename: user_identify/impl/token.txt
        grpc:
          envoy_grpc:
            cluster_name: grpc_cluster
    )";
  v3::UserIdentifyGlobal global_config;
  TestUtility::loadFromYaml(yaml,global_config);
  UserIdentifyFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);
  NiceMock<Server::Configuration::MockFactoryContext> context_;
  auto callback = factory.createFilterFactoryFromProto(*proto_config, "stat", context_);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamFilter(_));
  callback(filter_callback);

  auto filter_config = std::make_shared<UserIdentifyFilterGlobalConfig>(
      global_config, context_, "", "", "", "");
  EXPECT_TRUE(filter_config->identify());
}

// 创建路由配置
TEST(Factory, RouteConfig) {
  UserIdentifyFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, context, ProtobufMessage::getNullValidationVisitor());
}



} // namespace UserIdentify
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
