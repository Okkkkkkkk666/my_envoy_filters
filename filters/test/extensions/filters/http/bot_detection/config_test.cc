#include "test/mocks/server/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/mocks/api/mocks.h"
#include "test/test_common/utility.h"
#include "test/test_common/environment.h"

#include "filters/source/extensions/filters/http/bot_detection/config.h"
#include "filters/source/extensions/filters/http/bot_detection/bot_detection.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace BotDetection {

class Factory : public testing::Test {
public:
  Factory() = default;
  void SetUp() {

    ON_CALL(context_, api()).WillByDefault(testing::ReturnRef(api_));
    ON_CALL(api_, fileSystem()).WillByDefault(testing::ReturnRef(file_system_));
    ON_CALL(file_system_, fileReadToEnd(_)).WillByDefault((Invoke([&](const std::string& path) {
      auto res = api_ptr->fileSystem().fileReadToEnd(path);
      return res;
    })));
  }
  Api::ApiPtr api_ptr = Api::createApiForTest();
  NiceMock<Api::MockApi> api_;
  NiceMock<Filesystem::MockInstance> file_system_;
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;
  NiceMock<Server::Configuration::MockServerFactoryContext> context_;
};

// 验证全局配置是否创建正确
TEST_F(Factory, GlobalConfig) {
  BotDetectionFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyConfigProto();

  NiceMock<Server::Configuration::MockFactoryContext> context;

  auto callback = factory.createFilterFactoryFromProto(*proto_config, "stats", context);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamFilter(_));
  callback(filter_callback);
}

// 验证RouteSpecificFilterConfig是否正确
TEST_F(Factory, RouteSpecificFilterConfig) {
  const std::string yaml =R"(
    enable: true
    none_user_agent_mode: false
    matcher:
      allow_list: 
        - (Pingdom\.com\\)(\d+)\.(\d+)
        - (Pingdrom\.com\\)(\d+)\.(\d+)
        - (Pingdrrom\.com\\)(\d+)\.(\d+)
      deny_list:
        - (Pingdfom\.com\\)(\d+)\.(\d+)
    filename: /home/helloxd/envoy-filters/filters/source/extensions/filters/http/bot_detection/impl/bot_regex_list.txt
  )";

  BotDetectionFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);


  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, context_, ProtobufMessage::getNullValidationVisitor());
  const auto* config = dynamic_cast<const BotDetectionFilterRouteConfig*>(route_config.get());

  EXPECT_EQ(config->enable(), true);
  EXPECT_EQ(config->noneUserAgentMode(), false);
  EXPECT_EQ(config->matcher()->botList().size(), 14);
  EXPECT_EQ(config->matcher()->allowList().size(), 3);
  EXPECT_EQ(config->matcher()->denyList().size(), 1);
}

// 验证enable默认关闭，none_user_agent_mode默认关闭
TEST_F(Factory, DisableNoneuseragentmodeDisableByDefault) {
  const std::string yaml = R"(
    matcher:
      allow_list: (Pingdom\.com\\)(\d+)\.(\d+)
    none_user_agent_mode: false
    enable: false
    filename: /home/helloxd/envoy-filters/filters/source/extensions/filters/http/bot_detection/impl/bot_regex_list.txt
  )";
  BotDetectionFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);

  NiceMock<Server::Configuration::MockServerFactoryContext> server_context;
  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, server_context, ProtobufMessage::getNullValidationVisitor());
  const auto* config = dynamic_cast<const BotDetectionFilterRouteConfig*>(route_config.get());
  EXPECT_FALSE(config->enable());
  EXPECT_FALSE(config->noneUserAgentMode());
}

// 验证matcher中的deny_list默认为空
TEST_F(Factory, DenyListIsEmpty) {
  const std::string yaml = R"(
    matcher:
      allow_list: 
        - (Pingdom\.com\\)(\d+)\.(\d+)
    filename: /home/helloxd/envoy-filters/filters/source/extensions/filters/http/bot_detection/impl/bot_regex_list.txt
  )";

  BotDetectionFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);

  NiceMock<Server::Configuration::MockServerFactoryContext> server_context;
  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, server_context, ProtobufMessage::getNullValidationVisitor());
  const auto* config = dynamic_cast<const BotDetectionFilterRouteConfig*>(route_config.get());
  EXPECT_EQ(config->matcher()->denyList().empty(), true);
}

} // namespace BotDetection
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy