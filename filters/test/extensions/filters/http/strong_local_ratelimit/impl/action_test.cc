
#include <chrono>

#include "test/mocks/http/mocks.h"
#include "test/mocks/local_info/mocks.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filters/api/envoy/extensions/filters/http/strong_local_ratelimit/v3/strong_local_ratelimit.pb.h"
#include "filters/source/extensions/filters/http/strong_local_ratelimit/strong_local_ratelimit.h"
#include "source/common/network/address_impl.h"
#if 0
namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {

namespace v3 = envoy::extensions::filters::http::strong_local_ratelimit::v3;

static const std::string yaml = R"(
    enable: true
    dryrun: false
    rules:
      - src_ip:
          address_prefix: 127.0.0.1
          prefix_len: 24
        route_id: [0,1]
        upstream: service_envoyproxy_io
        action:
          target: {}
          quotas:
            - duration: {}
              max_count: {}
            - duration: {}
              max_count: {}
          after_pass: NEXT_RULE
)";

class ActionTest : public testing::Test {
public:
  ActionTest() = default;

  void setup(const std::string& yaml, v3::Action::Target target = v3::Action::ALL,
             uint32_t duration1 = 1, uint32_t max_count1 = 5, uint32_t duration2 = 60,
             uint32_t max_count2 = 30) {

    ON_CALL(decoder_callbacks_, dispatcher()).WillByDefault(ReturnRef(dispatcher_));

    envoy::extensions::filters::http::strong_local_ratelimit::v3::StrongLocalRateLimitRoute config;
    TestUtility::loadFromYaml(
        fmt::format(yaml, target, duration1, max_count1, duration2, max_count2), config);
    config_ = std::make_shared<FilterRouteConfig>(config, dispatcher_);
  }

  testing::NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Event::MockDispatcher> dispatcher_;
  Stats::IsolatedStoreImpl stats_;
  std::shared_ptr<FilterRouteConfig> config_;
};

// 验证流控项LRU是全局的，并且不会随着Action销毁而销毁
TEST_F(ActionTest, GlogQuotas) {
  auto headers = Http::TestRequestHeaderMapImpl();
  Impl::Action::ProcResult result;
  const size_t count = 100;
  Network::Address::InstanceConstSharedPtr ip =
      std::make_unique<Network::Address::Ipv4Instance>("192.168.1.68", 5678);
  for (size_t i = 0; i < count; ++i) {
    setup(yaml, v3::Action::ALL, 1, 5, 0, 0);
    uint32_t filter_instace_id = i;
    config_->rules().front()->action().proc(ip, headers, "", filter_instace_id, 0, false, result);
    ASSERT_TRUE(result.pass);
  }

  // 多次销毁Action
  for (size_t i = 0; i < 1000; ++i) {
    setup(yaml, v3::Action::ALL, 1, 5, 60, 30);
  }

  // 验证LRU数量
  ASSERT_EQ(Impl::Action::quotasSize(), count);
}

// 验证清理流控项是否正确
TEST_F(ActionTest, CleanQuotas) {
  const size_t count = 100;

  // 延时一下让缓存失效
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  ASSERT_EQ(Impl::Action::quotasSize(), count);
  Impl::Action::cleanQuotas(count);
  ASSERT_EQ(Impl::Action::quotasSize(), 0);
}

// 验证输入不同的配额区间，能否废弃无效的配额
TEST_F(ActionTest, QuotaConfig) {
  setup(yaml, v3::Action::ALL, 1, 5, 60, 30);
  ASSERT_EQ(config_->rules().size(), 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].duration, 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].max_count, 5);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].duration, 60);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].max_count, 30);

  setup(yaml, v3::Action::ALL, 60, 30, 1, 5);
  ASSERT_EQ(config_->rules().size(), 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].duration, 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].max_count, 5);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].duration, 60);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].max_count, 30);

  setup(yaml, v3::Action::ALL, 1, 5, 0, 0);
  ASSERT_EQ(config_->rules().size(), 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].duration, 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].max_count, 5);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].duration, 0);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].max_count, 0);

  setup(yaml, v3::Action::ALL, 0, 0, 1, 5);
  ASSERT_EQ(config_->rules().size(), 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].duration, 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].max_count, 5);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].duration, 0);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].max_count, 0);

  setup(yaml, v3::Action::ALL, 1, 5, 60, 5);
  ASSERT_EQ(config_->rules().size(), 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].duration, 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].max_count, 5);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].duration, 0);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].max_count, 0);

  setup(yaml, v3::Action::ALL, 1, 5, 60, 6);
  ASSERT_EQ(config_->rules().size(), 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].duration, 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].max_count, 5);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].duration, 60);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].max_count, 6);

  setup(yaml, v3::Action::ALL, 60, 300, 1, 5);
  ASSERT_EQ(config_->rules().size(), 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].duration, 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].max_count, 5);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].duration, 0);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].max_count, 0);

  setup(yaml, v3::Action::ALL, 60, 299, 1, 5);
  ASSERT_EQ(config_->rules().size(), 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].duration, 1);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[0].max_count, 5);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].duration, 60);
  ASSERT_EQ(config_->rules().front()->action().quotaConfig()[1].max_count, 299);
}

// 验证v3::Action::ALL时，是否正确
TEST_F(ActionTest, ProcAll) {
  setup(yaml, v3::Action::ALL, 1, 5, 0, 0);
  auto headers = Http::TestRequestHeaderMapImpl();
  Network::Address::InstanceConstSharedPtr ip =
      std::make_unique<Network::Address::Ipv4Instance>("192.16.88.8", 5678);
  std::string_view vh_name("vh_name1");
  uint32_t filter_instace_id = 0;
  uint64_t rule_hash = 1234;
  Impl::Action::ProcResult result;

  for (size_t i = 0; i < 5; i++) {
    config_->rules().front()->action().proc(ip, headers, vh_name, filter_instace_id, rule_hash,
                                            false, result);
    ASSERT_TRUE(result.pass);
  }
  config_->rules().front()->action().proc(ip, headers, vh_name, filter_instace_id, rule_hash, false,
                                          result);
  ASSERT_FALSE(result.pass);
  ip = std::make_unique<Network::Address::Ipv4Instance>("192.16.88.18", 5678);
  config_->rules().front()->action().proc(ip, headers, vh_name, filter_instace_id, rule_hash, false,
                                          result);
  ASSERT_FALSE(result.pass);
}

// 验证v3::Action::IP时，是否正确
TEST_F(ActionTest, ProcIp) {
  setup(yaml, v3::Action::IP, 1, 5, 0, 0);
  auto headers = Http::TestRequestHeaderMapImpl();
  Network::Address::InstanceConstSharedPtr ip =
      std::make_unique<Network::Address::Ipv4Instance>("0.8.8.8", 5678);
  Network::Address::InstanceConstSharedPtr ip2 =
      std::make_unique<Network::Address::Ipv4Instance>("0.8.8.9", 5678);
  std::string_view vh_name("vh_name1");
  uint32_t filter_instace_id = 0;
  uint64_t rule_hash = 1234;
  Impl::Action::ProcResult result;

  for (size_t i = 0; i < 5; i++) {
    config_->rules().front()->action().proc(ip, headers, vh_name, filter_instace_id, rule_hash,
                                            false, result);
    ASSERT_TRUE(result.pass);
    config_->rules().front()->action().proc(ip2, headers, vh_name, filter_instace_id, rule_hash,
                                            false, result);
    ASSERT_TRUE(result.pass);
  }
  config_->rules().front()->action().proc(ip, headers, vh_name, filter_instace_id, rule_hash, false,
                                          result);
  ASSERT_FALSE(result.pass);
  config_->rules().front()->action().proc(ip2, headers, vh_name, filter_instace_id, rule_hash,
                                          false, result);
  ASSERT_FALSE(result.pass);
}

// 验证v3::Action::HEADER时，是否正确
TEST_F(ActionTest, ProcHeader) {
  setup(yaml, v3::Action::HEADER, 1, 5, 0, 0);
  auto headers = Http::TestRequestHeaderMapImpl();
  Network::Address::InstanceConstSharedPtr ip =
      std::make_unique<Network::Address::Ipv4Instance>("1.8.8.8", 5678);
  Network::Address::InstanceConstSharedPtr ip2 =
      std::make_unique<Network::Address::Ipv4Instance>("1.8.8.9", 5678);
  std::string_view vh_name("vh_name1");
  uint32_t filter_instace_id = 0;
  uint64_t rule_hash = 1234;
  Impl::Action::ProcResult result;

  // 验证没有指定HEADER，则退化成按IP
  for (size_t i = 0; i < 5; i++) {
    config_->rules().front()->action().proc(ip, headers, vh_name, filter_instace_id, rule_hash,
                                            false, result);
    ASSERT_TRUE(result.pass);

    config_->rules().front()->action().proc(ip2, headers, vh_name, filter_instace_id, rule_hash,
                                            false, result);
    ASSERT_TRUE(result.pass);
  }
  config_->rules().front()->action().proc(ip, headers, vh_name, filter_instace_id, rule_hash, false,
                                          result);
  ASSERT_FALSE(result.pass);
  config_->rules().front()->action().proc(ip2, headers, vh_name, filter_instace_id, rule_hash,
                                          false, result);
  ASSERT_FALSE(result.pass);

  // 指定header
  const std::string yaml2 = R"(
    enable: true
    dryrun: false
    rules:
      - src_ip:
          address_prefix: 127.0.0.1
          prefix_len: 24
        route_id: [0,1]
        upstream: service_envoyproxy_io
        action:
          target: {}
          header:
            name: whoami
            present_match: true
          quotas:
            - duration: {}
              max_count: {}
            - duration: {}
              max_count: {}
          after_pass: NEXT_RULE
  )";
  setup(yaml2, v3::Action::HEADER, 1, 5, 0, 0);
  headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "i am LiLei");
  auto headers2 = Http::TestRequestHeaderMapImpl();
  headers2.setCopy(Envoy::Http::LowerCaseString("whoami"), "i am HanMeimei");

  for (size_t i = 0; i < 5; i++) {
    config_->rules().front()->action().proc(ip, headers, vh_name, filter_instace_id, rule_hash,
                                            false, result);
    ASSERT_TRUE(result.pass);

    config_->rules().front()->action().proc(ip, headers2, vh_name, filter_instace_id, rule_hash,
                                            false, result);
    ASSERT_TRUE(result.pass);
  }
  config_->rules().front()->action().proc(ip, headers, vh_name, filter_instace_id, rule_hash, false,
                                          result);
  ASSERT_FALSE(result.pass);
  config_->rules().front()->action().proc(ip, headers2, vh_name, filter_instace_id, rule_hash,
                                          false, result);
  ASSERT_FALSE(result.pass);
}

// 验证v3::Action::HEADER时，使用正则提取一段值是否正确
TEST_F(ActionTest, ProcHeaderRegex) {
  const std::string yaml = R"(
    enable: true
    dryrun: false
    rules:
      - src_ip:
          address_prefix: 127.0.0.1
          prefix_len: 24
        route_id: [0,1]
        upstream: service_envoyproxy_io
        action:
          target: {}
          header:
            name: whoami
            safe_regex_match: 
              google_re2:
                max_program_size: 100
              regex: "<.*>"
          quotas:
            - duration: {}
              max_count: {}
            - duration: {}
              max_count: {} 
          after_pass: NEXT_RULE
  )";

  setup(yaml, v3::Action::HEADER, 1, 5, 0, 0);
  auto headers = Http::TestRequestHeaderMapImpl();
  auto headers2 = Http::TestRequestHeaderMapImpl();

  Network::Address::InstanceConstSharedPtr ip =
      std::make_unique<Network::Address::Ipv4Instance>("2.8.8.8", 5678);
  Network::Address::InstanceConstSharedPtr ip2 =
      std::make_unique<Network::Address::Ipv4Instance>("2.8.8.9", 5678);
  std::string_view vh_name("vh_name1");
  uint32_t filter_instace_id = 0;
  uint64_t rule_hash = 1234;
  Impl::Action::ProcResult result;

  // 验证正则提取失败，则退化成按IP
  headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "i am LiLei");
  headers2.setCopy(Envoy::Http::LowerCaseString("whoami"), "i am HanMeimei");
  for (size_t i = 0; i < 5; i++) {
    config_->rules().front()->action().proc(ip, headers, vh_name, filter_instace_id, rule_hash,
                                            false, result);
    ASSERT_TRUE(result.pass);

    config_->rules().front()->action().proc(ip2, headers, vh_name, filter_instace_id, rule_hash,
                                            false, result);
    ASSERT_TRUE(result.pass);
  }
  config_->rules().front()->action().proc(ip, headers, vh_name, filter_instace_id, rule_hash, false,
                                          result);
  ASSERT_FALSE(result.pass);
  config_->rules().front()->action().proc(ip2, headers, vh_name, filter_instace_id, rule_hash,
                                          false, result);
  ASSERT_FALSE(result.pass);

  // 验证正则提取成功，则按header
  headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "i am <LiLei>");
  headers2.setCopy(Envoy::Http::LowerCaseString("whoami"), "i am <HanMeimei>");
  for (size_t i = 0; i < 5; i++) {
    config_->rules().front()->action().proc(ip, headers, vh_name, filter_instace_id, rule_hash,
                                            false, result);
    ASSERT_TRUE(result.pass);

    config_->rules().front()->action().proc(ip, headers2, vh_name, filter_instace_id, rule_hash,
                                            false, result);
    ASSERT_TRUE(result.pass);
  }
  config_->rules().front()->action().proc(ip, headers, vh_name, filter_instace_id, rule_hash, false,
                                          result);
  ASSERT_FALSE(result.pass);
  config_->rules().front()->action().proc(ip, headers2, vh_name, filter_instace_id, rule_hash,
                                          false, result);
  ASSERT_FALSE(result.pass);
}

} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
#endif