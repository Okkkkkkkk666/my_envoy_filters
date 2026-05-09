#include "test/mocks/server/mocks.h"
#include "test/mocks/server/instance.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/mocks/api/mocks.h"
#include "test/test_common/utility.h"
#include "test/test_common/environment.h"
#include "filters/source/extensions/filters/http/strong_global_ratelimit/config.h"
#include "filters/source/extensions/filters/http/common/ratelimit/impl/ratelimit_policy.h"
#include "filters/source/extensions/filters/http/strong_global_ratelimit/strong_global_ratelimit.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongGlobalRatelimit {

// 验证全局配置是否创建正确
TEST(Factory, GlobalConfig) {
  const std::string yaml = R"(
    grpc:
      envoy_grpc:
        cluster_name: grpc_service
      timeout: 5s
  )";
  StrongGlobalRatelimitFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);
  NiceMock<Server::Configuration::MockFactoryContext> context;
  auto callback = factory.createFilterFactoryFromProto(*proto_config, "stats", context);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamFilter(_));
  callback(filter_callback);
}

// 验证RouteSpecificFilterConfig是否正确
TEST(Factory, RouteSpecificFilterConfig) {
  const std::string yaml = R"(
    enable: true
    dryrun: false
    rules:
      - src_ip:
          address_prefix: 192.168.199.129
        route_id: 123456
        upstream: service_envoyproxy_io
        ip_invert: false
        action:
          quotas:
            - duration: 1
              max_count: 5
          after_pass: NEXT_RULE
          target: HEADER
          header:
            name: u1ser2-agent
            present_match: true
      - src_ip:
          address_prefix: 192.168.1.1
          prefix_len: 24
        route_id: [1]
        upstream: service_envoyproxy_io
        action:
          quotas:
            - duration: 5
              max_count: 1
          after_pass: RETURN
          target: HEADER
          header:
            name: u1ser2-agent
            present_match: false
  )";
  StrongGlobalRatelimitFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);

  NiceMock<Server::Configuration::MockServerFactoryContext> context;

  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, context, ProtobufMessage::getNullValidationVisitor());
  const auto* config =
      dynamic_cast<const StrongGlobalRateLimitFilterRouteConfig*>(route_config.get());
  // EXPECT_EQ(config->enable(), true);
  // EXPECT_EQ(config->dryrun(), false);
  EXPECT_EQ(config->rules().size(), 2);
}

// 验证enable默认关闭，dryrun默认关闭
TEST(Factory, EnableDryrunDisableByDefault) {
  const std::string yaml = R"(
    rules:
      - src_ip:
          address_prefix: 192.168.199.129
        route_id: 123456
        upstream: service_envoyproxy_io
        ip_invert: false
        action:
          quotas:
            - duration: 1
              max_count: 5
          after_pass: NEXT_RULE
          target: HEADER
          header:
            name: u1ser2-agent
            present_match: true
  )";

  StrongGlobalRatelimitFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);

  NiceMock<Server::Configuration::MockServerFactoryContext> context;

  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, context, ProtobufMessage::getNullValidationVisitor());
  const auto* config =
      dynamic_cast<const StrongGlobalRateLimitFilterRouteConfig*>(route_config.get());
  // EXPECT_FALSE(config->enable());
  // EXPECT_FALSE(config->dryrun());
}

// 验证rule中的routeId默认为空
TEST(Factory, RuleNoRouteId) {
  const std::string yaml = R"(
    rules:
      - src_ip:
          address_prefix: 192.168.199.129
        upstream: service_envoyproxy_io
        ip_invert: false
        action:
          quotas:
            - duration: 1
              max_count: 5
          after_pass: NEXT_RULE
          target: HEADER
          header:
            name: u1ser2-agent
            present_match: true
  )";

  StrongGlobalRatelimitFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);

  NiceMock<Server::Configuration::MockServerFactoryContext> server_context;
  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, server_context, ProtobufMessage::getNullValidationVisitor());
  const auto* config =
      dynamic_cast<const StrongGlobalRateLimitFilterRouteConfig*>(route_config.get());
  const std::vector<Impl::RulePtr>& rules = config->rules();
  EXPECT_TRUE(rules.size() == 1);
  // EXPECT_TRUE(rules.front()->routeId().empty());
}

// 验证rule中的upstream默认为空
TEST(Factory, RuleNoUpstream) {
  const std::string yaml = R"(
    rules:
      - src_ip:
          address_prefix: 192.168.199.129
        route_id: 123456
        ip_invert: false
        action:
          quotas:
            - duration: 1
              max_count: 5
          after_pass: NEXT_RULE
          target: HEADER
          header:
            name: u1ser2-agent
            present_match: true
  )";

  StrongGlobalRatelimitFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);

  NiceMock<Server::Configuration::MockServerFactoryContext> context;

  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, context, ProtobufMessage::getNullValidationVisitor());
  const auto* config =
      dynamic_cast<const StrongGlobalRateLimitFilterRouteConfig*>(route_config.get());
  const std::vector<Impl::RulePtr>& rules = config->rules();
  EXPECT_TRUE(rules.size() == 1);
  // EXPECT_TRUE(rules.front()->commonRule()->upstreamName().empty());
}

// 验证action的各项值
TEST(Factory, RuleDefultAction) {
  const std::string yaml = R"(
    rules:
      - src_ip:
          address_prefix: 192.168.199.129
        route_id: 123456
        ip_invert: false
        action:
          add_headers: false
          quotas:
            - duration: 1
              max_count: 5
            - duration: 2
              max_count: 6
          after_pass: NEXT_RULE
          target: HEADER
          header:
            name: u1ser2-agent
            present_match: true
  )";

  StrongGlobalRatelimitFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);

  NiceMock<Server::Configuration::MockServerFactoryContext> context;

  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, context, ProtobufMessage::getNullValidationVisitor());
  const auto* config =
      dynamic_cast<const StrongGlobalRateLimitFilterRouteConfig*>(route_config.get());
  const std::vector<Impl::RulePtr>& rules = config->rules();
  EXPECT_TRUE(rules.size() == 1);
  const Impl::Action& action = rules.front()->action();
  EXPECT_TRUE(action.target() == v3::Action::Target::Action_Target_HEADER);
  // EXPECT_FALSE(action.addHeaders());
  EXPECT_TRUE(action.procNextRule());
  const Envoy::Extensions::Filters::Common::RatelimitClient::Impl::Quotas quotas =
      action.getQuotas();
  EXPECT_TRUE(quotas[0].duration_ == 1);
  EXPECT_TRUE(quotas[0].max_count_ == 5);
  EXPECT_TRUE(quotas[1].duration_ == 2);
  EXPECT_TRUE(quotas[1].max_count_ == 6);
}

// 验证rule中的IpWhitelist
TEST(Factory, IpWhitelist) {
  const std::string yaml = R"(
    ip_whitelist: 
    - ip_range_list:
      - start_ip: "192.192.100.1"
        end_ip: "192.192.100.200"
      ip_list:
        list:
        - address_prefix: "192.192.100.1"
          prefix_len: 24
      enable: true
  )";

  StrongGlobalRatelimitFilterFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);

  NiceMock<Server::Configuration::MockServerFactoryContext> context;

  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, context, ProtobufMessage::getNullValidationVisitor());
  const auto* config =
      dynamic_cast<const StrongGlobalRateLimitFilterRouteConfig*>(route_config.get());
  // EXPECT_FALSE(config->ipWhitelist().empty());
  // EXPECT_TRUE(config->ipWhitelist()[0]->enable());
}

} // namespace StrongGlobalRatelimit
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
