
#include <chrono>
#include "test/mocks/http/mocks.h"
#include "test/mocks/local_info/mocks.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filters/source/extensions/filters/http/strong_global_ratelimit/strong_global_ratelimit.h"
#include "source/common/network/address_impl.h"
#if 0
namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongGlobalRatelimit {

namespace v3 = envoy::extensions::filters::http::strong_global_ratelimit::v3;

class RulesTest : public testing::Test {
public:
  RulesTest() = default;
  void setup(const std::string& yaml) {
    v3::StrongGlobalRateLimitRoute config;
    TestUtility::loadFromYaml(yaml, config);
    config_ = std::make_shared<StrongGlobalRateLimitFilterRouteConfig>(config);
  }
  testing::NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  std::shared_ptr<StrongGlobalRateLimitFilterRouteConfig> config_;
};

//route_id_set不为空，且route_id不存在
TEST_F(RulesTest,IDIsNotEmptyNoRouteid){
  const std::string yaml = R"(
    enable: true
    dryrun: false
    rules:
      - src_ip:
          address_prefix: 127.0.0.1
        route_id: 1234
        upstream: service_envoyproxy_io
        ip_invert: false
        action:
          quotas:
            - duration: 1
              max_count: 5
          after_pass: NEXT_RULE
          target: ALL
  )";  
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  Network::Address::InstanceConstSharedPtr ip =
      std::make_unique<Network::Address::Ipv4Instance>("192.16.88.8", 5678);
  std::string upstream("service_envoyproxy_io");
  uint64_t root_id=123;
  bool is_match=config_->rules().front()->match(root_id,upstream,ip,headers);
  EXPECT_FALSE(is_match);
}

//route_id_set_为空
TEST_F(RulesTest,RouteidsetIsEmpty){
  const std::string yaml = R"(
    enable: true
    dryrun: false
    rules:
      - ip_invert: false
        action:
          quotas:
            - duration: 1
              max_count: 5
          after_pass: NEXT_RULE
          target: ALL
  )";  
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  Network::Address::InstanceConstSharedPtr ip =
      std::make_unique<Network::Address::Ipv4Instance>("192.16.88.8", 5678);
  std::string upstream("service_envoyproxy_io");
  uint64_t root_id=1234;
  bool is_match=config_->rules().front()->match(root_id,upstream,ip,headers);
  EXPECT_FALSE(is_match);
}

//route_id_set_不为空且存在route_id
TEST_F(RulesTest,IDIsNotEmptyAndRouteid){
  const std::string yaml = R"(
    enable: true
    dryrun: false
    rules:
      - route_id: 1234
        ip_invert: false
        action:
          quotas:
            - duration: 1
              max_count: 5
          after_pass: NEXT_RULE
          target: ALL
  )";  
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  Network::Address::InstanceConstSharedPtr ip =
      std::make_unique<Network::Address::Ipv4Instance>("192.16.88.8", 5678);
  std::string upstream("service_envoyproxy_io");
  uint64_t root_id=1234;
  bool is_match=config_->rules().front()->match(root_id,upstream,ip,headers);
  EXPECT_TRUE(is_match);
}

} // namespace StrongGlobalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
#endif