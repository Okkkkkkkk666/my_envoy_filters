
#include "test/mocks/server/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filters/source/extensions/filters/http/strong_local_ratelimit/config.h"
#include "filters/source/extensions/filters/http/strong_local_ratelimit/strong_local_ratelimit.h"
#if 0
namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {

class ConfigTest : public testing::Test {
public:
  static Event::TimerPtr& getTimer() { return FilterGlobalConfig::timer_; }
};

// 验证全局配置是否创建正确
TEST(Factory, GlobalConfig) {
  StrongLocalRateLimitFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyConfigProto();

  NiceMock<Server::Configuration::MockFactoryContext> context;

  auto callback = factory.createFilterFactoryFromProto(*proto_config, "stats", context);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamFilter(_));
  callback(filter_callback);

  // FilterGlobalConfig::timer_是全局的，在leak检测时还没析构
  testing::Mock::AllowLeak(ConfigTest::getTimer().get());
}

// 验证RouteSpecificFilterConfig是否正确
TEST(Factory, RouteSpecificFilterConfig) {
  const std::string yaml = R"(
    enable: true
    dryrun: false
    rules:
      - src_ip:
          address_prefix: 127.0.0.1
          prefix_len: 24
        route_id: [0, 1]
        upstream: service_envoyproxy_io
        action:
          quotas:
            - duration: 1
              max_count: 5
            - duration: 60
              max_count: 30
          after_pass: NEXT_RULE
      - src_ip:
          address_prefix: 192.168.1.1
          prefix_len: 24
        route_id: [1]
        upstream: service_envoyproxy_io
        action:
          quotas:
            - duration: 5
              max_count: 1
            - duration: 60
              max_count: 10
          after_pass: RETURN
  )";

  StrongLocalRateLimitFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);

  NiceMock<Server::Configuration::MockServerFactoryContext> context;

  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, context, ProtobufMessage::getNullValidationVisitor());
  const auto* config = dynamic_cast<const FilterRouteConfig*>(route_config.get());
  EXPECT_EQ(config->enable(), true);
  EXPECT_EQ(config->dryrun(), false);
  EXPECT_EQ(config->rules().size(), 2);
}

// 验证enable默认关闭，dryrun默认关闭
TEST(Factory, EnableDryrunDisableByDefault) {
  const std::string yaml = R"(
    rules:
      - src_ip:
          address_prefix: 192.168.1.1
          prefix_len: 24
        route_id: [1]
        upstream: service_envoyproxy_io
        action:
          quotas:
            - duration: 5
              max_count: 1
            - duration: 1
              max_count: 50
          after_pass: RETURN
  )";

  StrongLocalRateLimitFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);

  NiceMock<Server::Configuration::MockServerFactoryContext> server_context;
  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, server_context, ProtobufMessage::getNullValidationVisitor());
  const auto* config = dynamic_cast<const FilterRouteConfig*>(route_config.get());
  EXPECT_FALSE(config->enable());
  EXPECT_FALSE(config->dryrun());
}

// 验证rule中的routeId默认为空
TEST(Factory, RuleNoRouteId) {
  const std::string yaml = R"(
    rules:
      - src_ip:
          address_prefix: 192.168.1.1
          prefix_len: 24
        upstream: service_envoyproxy_io
        action:
          quotas:
            - duration: 5
              max_count: 1
            - duration: 1
              max_count: 50
          after_pass: RETURN
  )";

  StrongLocalRateLimitFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);

  NiceMock<Server::Configuration::MockServerFactoryContext> server_context;
  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, server_context, ProtobufMessage::getNullValidationVisitor());
  const auto* config = dynamic_cast<const FilterRouteConfig*>(route_config.get());
  const std::vector<Impl::RulePtr>& rules = config->rules();
  EXPECT_TRUE(rules.size() == 1);
  EXPECT_TRUE(rules.front()->routeId().empty());
}

// 验证rule中的upstream默认为空
TEST(Factory, RuleNoUpstream) {
  const std::string yaml = R"(
    rules:
      - src_ip:
          address_prefix: 192.168.1.1
          prefix_len: 24
        action:
          quotas:
            - duration: 5
              max_count: 1
            - duration: 1
              max_count: 50
          after_pass: RETURN
  )";

  StrongLocalRateLimitFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);

  NiceMock<Server::Configuration::MockServerFactoryContext> server_context;
  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, server_context, ProtobufMessage::getNullValidationVisitor());
  const auto* config = dynamic_cast<const FilterRouteConfig*>(route_config.get());
  const std::vector<Impl::RulePtr>& rules = config->rules();
  EXPECT_TRUE(rules.size() == 1);
  EXPECT_TRUE(rules.front()->commonRule()->upstreamName().empty());
}

// 验证action的各项默认值
TEST(Factory, RuleNoAction) {
  const std::string yaml = R"(
    rules:
      - src_ip:
          address_prefix: 192.168.1.1
          prefix_len: 24
        route_id: [1]
        upstream: service_envoyproxy_io
  )";

  StrongLocalRateLimitFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);

  NiceMock<Server::Configuration::MockServerFactoryContext> server_context;
  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, server_context, ProtobufMessage::getNullValidationVisitor());
  const auto* config = dynamic_cast<const FilterRouteConfig*>(route_config.get());
  const std::vector<Impl::RulePtr>& rules = config->rules();
  EXPECT_TRUE(rules.size() == 1);
  const Impl::Action& action = rules.front()->action();
  EXPECT_TRUE(action.target() == v3::Action::Target::Action_Target_ALL);
  EXPECT_FALSE(action.addHeaders());
  EXPECT_FALSE(action.procNextRule());
  const std::array<Impl::Action::QuotaConfig, 2>& quota_config = action.quotaConfig();
  for (const Impl::Action::QuotaConfig& config : quota_config) {
    EXPECT_TRUE(config.duration == 0);
    EXPECT_TRUE(config.max_count == 0);
  }
}

} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
#endif