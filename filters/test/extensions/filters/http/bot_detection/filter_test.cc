#include <chrono>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/test_common/utility.h"
#include "test/mocks/local_info/mocks.h"
#include "test/mocks/upstream/cluster_info.h"

#include "envoy/config/route/v3/route_components.pb.h"

#include "source/common/json/json_loader.h"
#include "source/common/network/address_impl.h"

#include "filters/api/envoy/extensions/filters/http/bot_detection/v3/bot_detection.pb.h"
#include "filters/source/extensions/filters/http/bot_detection/bot_detection.h"
#include "filters/source/extensions/filters/http/bot_detection/config.h"

using testing::ReturnRef;

namespace Envoy {
namespace Router {
class MockVirtualHostImpl : public VirtualHostImpl {
public:
  MockVirtualHostImpl(const envoy::config::route::v3::VirtualHost& virtual_host,
                      NiceMock<Server::Configuration::MockServerFactoryContext>& context,
                      Stats::Scope& scope, ConfigImpl& config)
      : VirtualHostImpl(virtual_host, OptionalHttpFilters(), config, context, scope,
                        ProtobufMessage::getNullValidationVisitor(),
                        absl::optional<Upstream::ClusterManager::ClusterInfoMaps>()){};
  ~MockVirtualHostImpl() override = default;

  // Router::VirtualHost
  MOCK_METHOD(const std::string&, name, (), (const));
  MOCK_METHOD(const RouteSpecificFilterConfig*, perFilterConfig, (const std::string&), (const));
  MOCK_METHOD(bool, includeAttemptCountInRequest, (), (const));
  MOCK_METHOD(bool, includeAttemptCountInResponse, (), (const));
  MOCK_METHOD(Upstream::RetryPrioritySharedPtr, retryPriority, ());
  MOCK_METHOD(Upstream::RetryHostPredicateSharedPtr, retryHostPredicate, ());
  MOCK_METHOD(uint32_t, retryShadowBufferLimit, (), (const));

  mutable Stats::TestUtil::TestSymbolTable symbol_table_;
  std::string name_{"fake_vhost"};
  mutable std::unique_ptr<Stats::StatNameManagedStorage> stat_name_;
};

class MyMockRoute : public PathRouteEntryImpl {
public:
  MyMockRoute(const VirtualHostImpl& vhost, const envoy::config::route::v3::Route& route,
              const OptionalHttpFilters& optional_http_filters,
              Server::Configuration::ServerFactoryContext& factory_context,
              ProtobufMessage::ValidationVisitor& validator)
      : PathRouteEntryImpl(vhost, route, optional_http_filters, factory_context, validator) {}
  ~MyMockRoute() = default;

  // Router::Route
  MOCK_METHOD(const DirectResponseEntry*, directResponseEntry, (), (const));
  MOCK_METHOD(const RouteEntry*, routeEntry, (), (const));
  MOCK_METHOD(const Decorator*, decorator, (), (const));
  MOCK_METHOD(const RouteTracing*, tracingConfig, (), (const));
  MOCK_METHOD(const RouteSpecificFilterConfig*, perFilterConfig, (const std::string&), (const));
  MOCK_METHOD(const RouteSpecificFilterConfig*, mostSpecificPerFilterConfig, (const std::string&),
              (const));
  MOCK_METHOD(void, traversePerFilterConfig,
              (const std::string&, std::function<void(const Router::RouteSpecificFilterConfig&)>),
              (const));
  MOCK_METHOD(const envoy::config::core::v3::Metadata&, metadata, (), (const));
  MOCK_METHOD(const Envoy::Config::TypedMetadata&, typedMetadata, (), (const));

  testing::NiceMock<MockRouteEntry> route_entry_;
  testing::NiceMock<MockDecorator> decorator_;
  testing::NiceMock<MockRouteTracing> route_tracing_;
  envoy::config::core::v3::Metadata metadata_;
};

} // namespace Router
} // namespace Envoy

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace BotDetection {

namespace v3 = envoy::extensions::filters::http::bot_detection::v3;

class FilterTest : public testing::Test {
public:
  FilterTest() = default;

  void setup(const std::string& yaml) {
    v3::BotDetectionRoute route_config;
    if (!yaml.empty()) {
      TestUtility::loadFromYaml(yaml, route_config);
      ON_CALL(context_, api()).WillByDefault(testing::ReturnRef(api_));
      ON_CALL(api_, fileSystem()).WillByDefault(testing::ReturnRef(file_system_));
      ON_CALL(file_system_, fileReadToEnd(_)).WillByDefault((Invoke([&](const std::string& path) {
        auto res = api_ptr->fileSystem().fileReadToEnd(path);
        return res;
      })));
    }
    bot_list_ = context_.api().fileSystem().fileReadToEnd(route_config.filename());
    route_config_ = std::make_shared<BotDetectionFilterRouteConfig>(route_config, bot_list_);

    v3::BotDetectionGlobal global_config;
    global_config_ = std::make_shared<BotDetectionFilterGlobalConfig>(global_config);

    ON_CALL(*(decoder_callbacks_.route_), mostSpecificPerFilterConfig(_))
        .WillByDefault(Return(route_config_.get()));
    filter_ = std::make_shared<BotDetectionFilter>(global_config_, context_);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
  }

  NiceMock<Server::Configuration::MockServerFactoryContext> context_;
  testing::NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  std::shared_ptr<BotDetectionFilterRouteConfig> route_config_;
  std::shared_ptr<BotDetectionFilterGlobalConfig> global_config_;
  std::shared_ptr<BotDetectionFilter> filter_;
  std::string bot_list_;
  NiceMock<Stats::MockIsolatedStatsStore> stats_;
  std::unique_ptr<Router::VirtualHostImpl> virtual_host_;
  BotDetectionFilterFactory factory_;
  Api::ApiPtr api_ptr = Api::createApiForTest();
  NiceMock<Api::MockApi> api_;
  NiceMock<Filesystem::MockInstance> file_system_;

};

// 验证没有VH配置时，不会执行
TEST_F(FilterTest, NoPerFilterConfigOverride) {
  setup("");
  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
}

// 验证Enable未启用时，不会执行
TEST_F(FilterTest, Disable) {
  const std::string yaml_local = R"(
    matcher: 
      allow_list: (Pingdom\.com\\)(\d+)\.(\d+)
    none_user_agent_mode: true
    enable: false
    filename: /home/helloxd/envoy-filters/filters/source/extensions/filters/http/bot_detection/impl/bot_regex_list.txt
  )";
  setup(yaml_local);
  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
}

// 验证当user-agent为空且none_user_agent_mode=true时,应该放行
TEST_F(FilterTest, NoUseragentAndAllow) {
  const std::string yaml_local = R"(
    matcher: 
      allow_list: (Pingdom\.com\\)(\d+)\.(\d+)
      # deny_list:
    none_user_agent_mode: true
    enable: true
    filename: /home/helloxd/envoy-filters/filters/source/extensions/filters/http/bot_detection/impl/bot_regex_list.txt
  )";
  setup(yaml_local);
  Http::TestRequestHeaderMapImpl sample_request{
      {":authority", "localhost:8888"},
      {":path", "/anything?aa=bb&cc=dd"},
      {":method", "GET"},
      {":scheme", "http"},
      // {"user-agent", " "},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
      {"x-envoy-expected-rq-timeout-ms", "15000"},
      {"foo", "bar"},
      {"end-user", "envoy"},
  };
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(sample_request, false));
}

// 验证当user-agent为空且none_user_agent_mode=false时，应该拒绝
TEST_F(FilterTest, NoUseragentAndDeny) {
  const std::string yaml_local = R"(
    matcher: 
      allow_list: (Pingdom\.com\\)(\d+)\.(\d+)
    none_user_agent_mode: false
    enable: true
    filename: /home/helloxd/envoy-filters/filters/source/extensions/filters/http/bot_detection/impl/bot_regex_list.txt
  )";
  setup(yaml_local);
  Http::TestRequestHeaderMapImpl sample_request{
      {":authority", "localhost:8888"},
      {":path", "/anything?aa=bb&cc=dd"},
      {":method", "GET"},
      {":scheme", "http"},
      // {"user-agent", ""},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
      {"x-envoy-expected-rq-timeout-ms", "15000"},
      {"foo", "bar"},
      {"end-user", "envoy"},
  };
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(sample_request, false));
}

// 验证匹配到的user-agent为黑名单时，拒绝请求
TEST_F(FilterTest, UserAgentIsDenyList) {
  const std::string yaml_local = R"(
    matcher: 
      deny_list: (Pingdom\.com\\)(\d+)\.(\d+)
    none_user_agent_mode: false
    enable: true
    filename: /home/helloxd/envoy-filters/filters/source/extensions/filters/http/bot_detection/impl/bot_regex_list.txt
  )";
  setup(yaml_local);
  Http::TestRequestHeaderMapImpl sample_request{
      {":authority", "localhost:8888"},
      {":path", "/anything?aa=bb&cc=dd"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "Pingdom.com\\1.1"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
      {"x-envoy-expected-rq-timeout-ms", "15000"},
      {"foo", "bar"},
      {"end-user", "envoy"},
  };
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(sample_request, false));
}

// 验证匹配到的user-agent为白名单时，放行请求
TEST_F(FilterTest, UserAgentIsAllowList) {
  const std::string yaml_local = R"(
    matcher: 
      allow_list: (Pingdom\.com\\)(\d+)\.(\d+)
    none_user_agent_mode: false
    enable: true
    filename: /home/helloxd/envoy-filters/filters/source/extensions/filters/http/bot_detection/impl/bot_regex_list.txt
  )";
  setup(yaml_local);
  Http::TestRequestHeaderMapImpl sample_request{
      {":authority", "localhost:8888"},
      {":path", "/anything?aa=bb&cc=dd"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "Pingdom.com\\1.1"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
      {"x-envoy-expected-rq-timeout-ms", "15000"},
      {"foo", "bar"},
      {"end-user", "envoy"},
  };
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(sample_request, false));
}

// 验证匹配到的user-agent为机器人名单时，拒绝请求
TEST_F(FilterTest, UserAgentIsBotList) {
  const std::string yaml_local = R"(
    none_user_agent_mode: false
    enable: true
    filename: /home/helloxd/envoy-filters/filters/source/extensions/filters/http/bot_detection/impl/bot_regex_list.txt
  )";
  setup(yaml_local);
  Http::TestRequestHeaderMapImpl sample_request{
      {":authority", "localhost:8888"},
      {":path", "/anything?aa=bb&cc=dd"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "Pingdom.com_bot_version_13.0"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
      {"x-envoy-expected-rq-timeout-ms", "15000"},
      {"foo", "bar"},
      {"end-user", "envoy"},
  };
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(sample_request, false));
}

// 验证没有匹配到user-agent时，放行请求
TEST_F(FilterTest, NoMatchUserAgent) {
  const std::string yaml_local = R"(
    matcher: 
      deny_list: (Pingdom\.com\\)(\d+)\.(\d+)
      allow_list: (Pingdom\.com\\)(\d+)
    none_user_agent_mode: false
    enable: true
    filename: /home/helloxd/envoy-filters/filters/source/extensions/filters/http/bot_detection/impl/bot_regex_list.txt
  )";
  setup(yaml_local);
  Http::TestRequestHeaderMapImpl sample_request{
      {":authority", "localhost:8888"},
      {":path", "/anything"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "aaaaaasssssssseeeeeee"},
      {"accept", "*/*"},
      {"end-user", "envoy"},
  };
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(sample_request, false));
}

} // namespace BotDetection
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
