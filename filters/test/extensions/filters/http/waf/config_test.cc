#include "test/mocks/server/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filters/source/extensions/filters/http/waf/config.h"
#include "filters/source/extensions/filters/http/waf/waf.h"
using testing::_;
namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WafFilter {

namespace v3 = envoy::extensions::filters::http::waf::v3;

class WafConfigTest : public testing::Test {
public:
  WafConfigTest() = default;

  std::shared_ptr<FilterGlobalConfig> makeConfig(const std::string& yaml) {
    v3::WafGlobal proto;
    TestUtility::loadFromYamlAndValidate(yaml, proto);
    return std::make_shared<FilterGlobalConfig>(proto, stats_prefix_, context_);
  }

protected:
  std::string stats_prefix_;
  NiceMock<Server::Configuration::MockFactoryContext> context_;
};

// 验证全局配置是否创建正确
TEST(Factory, GlobalConfig) {
  const std::string yaml = R"(
    paranoia_level: L3
    rule_types: 1040383
    detection_only: false
    mode: precise
    disable_rules:
      - id: Srhino-10292020
        level: 4
      - id: Srhino-10292037
        level: 4
    pass_rules:
      - id: Srhino-10291104
        level: 1
      - id: Srhino-10291105
        level: 1
    grpc:
      envoy_grpc:
        cluster_name: grpc_service
      timeout: 20s
  )";
  WafFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);
  NiceMock<Server::Configuration::MockFactoryContext> context;
  Http::FilterFactoryCb callback =
      factory.createFilterFactoryFromProto(*proto_config, "stats", context);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamFilter(_));
  callback(filter_callback);
}

// 验证检测模式配置
TEST_F(WafConfigTest, GlobalDetectConfig) {
  const std::string yaml = R"(
    paranoia_level: L4
    rule_types: 134209535
    detection_only: true
    mode: precise
  )";
  auto config = makeConfig(yaml);
  EXPECT_EQ(config->paranoia_level(), 3);
  EXPECT_EQ(config->ruleTypes(), 134217727);
  EXPECT_EQ(config->detectionOnly(), true);
  EXPECT_EQ(config->mode(), 1);
}

// 验证规则文件相关的默认路径配置
TEST_F(WafConfigTest, GlobalDefaultPathConfig) {
  const std::string yaml = R"(
    paranoia_level: L4
    rule_types: 134209535
    mode: precise
  )";
  auto config = makeConfig(yaml);
  EXPECT_EQ(config->rules_path_before(), "plugins/waf/conf/base/before.conf");
  EXPECT_EQ(config->rules_path_after(), "plugins/waf/conf/base/after.conf");
  EXPECT_EQ(config->tar_rules_path(), "plugins/waf/conf/waf_rule.tar.gz");
}

// 验证不设置验证攻击类型时，必要加载项是否加载
TEST_F(WafConfigTest, GlobalEmptyAttackTypeConfig) {
  const std::string yaml = R"(
    paranoia_level: L4
    rule_types: 0
    grpc:
      envoy_grpc:
        cluster_name: grpc_service
  )";
  v3::WafGlobal proto_config;
  auto config = makeConfig(yaml);
  EXPECT_EQ(config->paranoia_level(), 3);
  EXPECT_TRUE(config->ruleTypes() &
              v3::WafGlobal_RuleType::WafGlobal_RuleType_RT_BlockingEvaluation);
  // size_t rule_sizes = 0;
  // for (int i = 0; i <= modsecurity::Phases::NUMBER_OF_PHASES; i++) {
  //   rule_sizes += config->modsec_rules()->m_rulesSetPhases[i]->size();
  // }
  // EXPECT_EQ(rule_sizes, 176);
}

// 验证RouteSpecificFilterConfig配置创建是否正确
TEST_F(WafConfigTest, RouteSpecificFilterConfig) {
  const std::string yaml = R"(
    enable: true
    dryrun: false
    waf_ratelimit: true
    rules:
      - condition:
        - prefix: /
          case_sensitive: true
        action:
          quotas:
            - duration: 60
              max_count: 10
          target: IP_AND_PATH
          add_headers: true
  )";
  WafFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);
  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, context, ProtobufMessage::getNullValidationVisitor());
  EXPECT_TRUE(route_config.get());
  const auto* config = dynamic_cast<const FilterRouteConfig*>(route_config.get());
  EXPECT_TRUE(config);
}

// 验证限速相关配置是否正确
TEST_F(WafConfigTest, RouteRatelimitConfig) {
  const std::string yaml = R"(
    enable: true
    dryrun: false
    waf_ratelimit: true
    rules:
      - condition:
        - prefix: /
          case_sensitive: true
        action:
          quotas:
            - duration: 60
              max_count: 10
          target: IP_AND_PATH
          add_headers: true
  )";
  WafFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);
  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, context, ProtobufMessage::getNullValidationVisitor());
  EXPECT_TRUE(route_config.get());
  const auto* config = dynamic_cast<const FilterRouteConfig*>(route_config.get());
  EXPECT_TRUE(config);
  EXPECT_EQ(config->ratelimit_route_config()->enable(), true);
  EXPECT_EQ(config->ratelimit_route_config()->dryrun(), false);
  EXPECT_EQ(config->ratelimit_route_config()->rules()[0]->action().getQuotas()[0].duration_, 60);
  EXPECT_EQ(config->ratelimit_route_config()->rules()[0]->action().getQuotas()[0].max_count_, 10);
  EXPECT_EQ(config->ratelimit_route_proto_config().external_invoke(), true);
}

} // namespace WafFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
