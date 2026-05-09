
#include <chrono>
#include<string_view>
#include "test/mocks/http/mocks.h"
#include "test/mocks/local_info/mocks.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3/strong_global_ratelimit.pb.h"
#include "filters/source/extensions/filters/http/strong_global_ratelimit/strong_global_ratelimit.h"
#include "source/common/network/address_impl.h"
#if 0
namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongGlobalRatelimit {

namespace v3 = envoy::extensions::filters::http::strong_global_ratelimit::v3;

class ActionTest : public testing::Test {
public:
  ActionTest() = default;

  void setup(const std::string& yaml) {
    v3::StrongGlobalRateLimitRoute config;
    TestUtility::loadFromYaml(yaml, config);
    config_ = std::make_shared<StrongGlobalRateLimitFilterRouteConfig>(config);
  }

  testing::NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  std::shared_ptr<StrongGlobalRateLimitFilterRouteConfig> config_;
};

// 验证v3::Action::ALL时，是否正确
TEST_F(ActionTest, makeKeyAll) {
  const std::string yaml = R"(
    enable: true
    dryrun: false
    rules:
      - src_ip:
          address_prefix: 127.0.0.1
        route_id: 123456
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
  std::string vh_name("vh_name");
  uint32_t filter_instace_id = 0;
  uint64_t rule_hash = 1234;
  auto str=config_->rules().front()->action().makeKey(ip, headers, vh_name, filter_instace_id, rule_hash);
  EXPECT_STREQ(str.c_str(),"0vh_name12340");
}

// 验证v3::Action::IP时，是否正确
TEST_F(ActionTest, makeKeyIp) {
  const std::string yaml = R"(
    enable: true
    dryrun: false
    rules:
      - src_ip:
          address_prefix: 127.0.0.1
        route_id: 123456
        upstream: service_envoyproxy_io
        ip_invert: false
        action:
          quotas:
            - duration: 1
              max_count: 5
          after_pass: NEXT_RULE
          target: IP
  )";  
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  Network::Address::InstanceConstSharedPtr ip =
      std::make_unique<Network::Address::Ipv4Instance>("192.16.88.8", 5678);
  std::string vh_name("vh_name");
  uint32_t filter_instace_id = 0;
  uint64_t rule_hash = 1234;
  auto str=config_->rules().front()->action().makeKey(ip, headers, vh_name, filter_instace_id, rule_hash);
  EXPECT_STREQ(str.c_str(),"1192.16.88.8vh_name12340");
}

// 验证v3::Action::HEADER时，是否正确
TEST_F(ActionTest, makeKeyHeader) {
  const std::string yaml = R"(
    enable: true
    dryrun: false
    rules:
      - src_ip:
          address_prefix: 127.0.0.1
        route_id: 123456
        upstream: service_envoyproxy_io
        ip_invert: false
        action:
          quotas:
            - duration: 1
              max_count: 5
          after_pass: NEXT_RULE
          target: HEADER
  )";  
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  Network::Address::InstanceConstSharedPtr ip =
      std::make_unique<Network::Address::Ipv4Instance>("1.8.8.8", 5678);
  Network::Address::InstanceConstSharedPtr ip2 =
      std::make_unique<Network::Address::Ipv4Instance>("1.8.8.9", 5678);
  std::string vh_name("vh_name");
  uint32_t filter_instace_id = 0;
  uint64_t rule_hash = 1234;

  // 验证没有指定HEADER，则退化成按IP
  auto str1=config_->rules().front()->action().makeKey(ip, headers, vh_name, filter_instace_id, rule_hash);
  EXPECT_STREQ(str1.c_str(),"11.8.8.8vh_name12340");

  auto str2=config_->rules().front()->action().makeKey(ip2, headers, vh_name, filter_instace_id, rule_hash);
  EXPECT_STREQ(str2.c_str(),"11.8.8.9vh_name12340");

  // 指定header
  const std::string yaml2 = R"(
    enable: true
    dryrun: false
    rules:
      - src_ip:
          address_prefix: 127.0.0.1
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
  setup(yaml2);
  headers.setCopy(Envoy::Http::LowerCaseString("u1ser2-agent"), "one");
  auto headers2 = Http::TestRequestHeaderMapImpl();
  headers2.setCopy(Envoy::Http::LowerCaseString("u1ser2-agent"), "two");

  auto str3=config_->rules().front()->action().makeKey(ip, headers, vh_name, filter_instace_id, rule_hash);
  EXPECT_STREQ(str3.c_str(),"2onevh_name12340");

  auto str4=config_->rules().front()->action().makeKey(ip, headers2, vh_name, filter_instace_id, rule_hash);
  EXPECT_STREQ(str4.c_str(),"2twovh_name12340");
}

// 验证v3::Action::HEADER时，使用正则提取一段值是否正确
TEST_F(ActionTest, makeKeyHeaderRegex) {
  const std::string yaml = R"(
    enable: true
    dryrun: false
    rules:
      - src_ip:
          address_prefix: 127.0.0.1
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
            name: whoami
            safe_regex_match: 
              google_re2:
                max_program_size: 100
              regex: "<.*>"

  )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  auto headers2 = Http::TestRequestHeaderMapImpl();

  Network::Address::InstanceConstSharedPtr ip =
      std::make_unique<Network::Address::Ipv4Instance>("2.8.8.8", 5678);
  Network::Address::InstanceConstSharedPtr ip2 =
      std::make_unique<Network::Address::Ipv4Instance>("2.8.8.9", 5678);
  std::string vh_name("vh_name");
  uint32_t filter_instace_id = 0;
  uint64_t rule_hash = 1234;


  // 验证正则提取失败，则退化成按IP
  headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "i am LiLei");
  headers2.setCopy(Envoy::Http::LowerCaseString("whoami"), "i am HanMeimei");
 
  auto str1=config_->rules().front()->action().makeKey(ip, headers, vh_name, filter_instace_id, rule_hash);
  EXPECT_STREQ(str1.c_str(),"12.8.8.8vh_name12340");

  auto str2=config_->rules().front()->action().makeKey(ip2, headers, vh_name, filter_instace_id, rule_hash);
  EXPECT_STREQ(str2.c_str(),"12.8.8.9vh_name12340");

  // 验证正则提取成功，则按header
  headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "i am <LiLei>");
  headers2.setCopy(Envoy::Http::LowerCaseString("whoami"), "i am <HanMeimei>");

  auto str3=config_->rules().front()->action().makeKey(ip, headers, vh_name, filter_instace_id, rule_hash);
  EXPECT_STREQ(str3.c_str(),"2<LiLei>vh_name12340");

  auto str4=config_->rules().front()->action().makeKey(ip, headers2, vh_name, filter_instace_id, rule_hash);
  EXPECT_STREQ(str4.c_str(),"2<HanMeimei>vh_name12340");
}

// 验证v3::Action::Action_Target_IP_AND_PATH时，是否正确
TEST_F(ActionTest, makeKeyIPAndPath) {
  const std::string yaml = R"(
    enable: true
    dryrun: false
    rules:
      - src_ip:
          address_prefix: 127.0.0.1
        route_id: 123456
        upstream: service_envoyproxy_io
        ip_invert: false
        action:
          quotas:
            - duration: 1
              max_count: 5
          after_pass: NEXT_RULE
          target: IP_AND_PATH
  )";  
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("/login");
  Network::Address::InstanceConstSharedPtr ip =
      std::make_unique<Network::Address::Ipv4Instance>("192.16.88.8", 5678);
  std::string vh_name("vh_name");
  uint32_t filter_instace_id = 0;
  uint64_t rule_hash = 1234;
  auto key=config_->rules().front()->action().makeKey(ip, headers, vh_name, filter_instace_id, rule_hash);
  EXPECT_STREQ(key.c_str(),"3192.16.88.8/loginvh_name12340");
}

} // namespace StrongGlobalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
#endif