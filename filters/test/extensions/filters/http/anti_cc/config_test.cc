#include "test/mocks/server/factory_context.h"
#include "test/mocks/server/instance.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filters/api/envoy/extensions/filters/http/anti_cc/v3/anti_cc.pb.h"
#include "filters/api/envoy/extensions/filters/http/anti_cc/v3/anti_cc.pb.validate.h"

#include "filters/source/extensions/filters/http/anti_cc/anti_cc.h"
#include "filters/source/extensions/filters/http/anti_cc/config.h"

using testing::_;
using testing::NiceMock;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AntiCC {

namespace v3 = envoy::extensions::filters::http::anti_cc::v3;

TEST(AntiCCFilterConfigFactoryTest, AntiCCFilterEmptyProto) {
  AntiCCFilterFactory factory;
  const std::string yaml = R"(
      external_grpc_server:
        envoy_grpc:
          cluster_name: grpc_service
        timeout: 1s
  )";
  v3::AntiCCGlobal global_proto;
  TestUtility::loadFromYamlAndValidate(yaml, global_proto);
  NiceMock<Server::Configuration::MockFactoryContext> context;
  Http::FilterFactoryCb cb = factory.createFilterFactoryFromProto(global_proto, "stats", context);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamFilter(_));
  cb(filter_callback);
}

TEST(AntiCCFilterConfigFactoryTest, GlobalConfig) {
  const std::string yaml = R"(
      mode: CUSTOM
      action:
        dryrun: false
        man_machine_verification: true
        check_quota:
          duration: 30
          max_count: 3
        rate_limit_quota:
          duration: 30
          max_count: 3
        block_time: 500
      external_grpc_server:
        envoy_grpc:
          cluster_name: grpc_service
        timeout: 1s
      verification_server:
        cluster_name: verify_service
        timeout: 1s
      polycube_server:
        cluster_name: polycube_service
        timeout: 1s
      ip_whitelist:
      - ip_list:
          list:
          - address_prefix: "10.200.200.220"
        enable: true
  )";
  v3::AntiCCGlobal global_proto;
  TestUtility::loadFromYamlAndValidate(yaml, global_proto);
  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  FilterGlobalConfigSharedPtr filter_config =
      std::make_shared<AntiCCFilterGlobalConfig>(global_proto, context);
  EXPECT_EQ(filter_config->mode(), v3::Mode::CUSTOM);
  EXPECT_TRUE(filter_config->action()->manMachineVerification());
  EXPECT_FALSE(filter_config->action()->dryrun());
  EXPECT_EQ(filter_config->action()->verifyQuota().duration_, 30);
  EXPECT_EQ(filter_config->action()->verifyQuota().max_count_, 3);
  Envoy::Extensions::Filters::Common::RatelimitClient::Impl::Quotas quotas =
      filter_config->action()->rateLimitQuota();
  EXPECT_EQ(quotas[0].duration_, 30);
  EXPECT_EQ(quotas[0].max_count_, 3);
  EXPECT_EQ(filter_config->action()->blockTime(), 500);
}

// 验证dryrun默认关闭，man_machine_verification默认关闭
TEST(AntiCCFilterConfigFactoryTest, DryrunAndManMachineVerificationDisableByDefault) {
  const std::string yaml = R"(
      mode: CUSTOM
      action:
        rate_limit_quota:
          duration: 30
          max_count: 3
        block_time: 500
      external_grpc_server:
        envoy_grpc:
          cluster_name: grpc_service
        timeout: 1s
      verification_server:
        cluster_name: verify_service
        timeout: 1s
      polycube_server:
        cluster_name: polycube_service
        timeout: 1s
  )";
  v3::AntiCCGlobal global_proto;
  TestUtility::loadFromYamlAndValidate(yaml, global_proto);
  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  FilterGlobalConfigSharedPtr filter_config =
      std::make_shared<AntiCCFilterGlobalConfig>(global_proto, context);
  EXPECT_FALSE(filter_config->action()->manMachineVerification());
  EXPECT_FALSE(filter_config->action()->dryrun());
}

} // namespace AntiCC
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy