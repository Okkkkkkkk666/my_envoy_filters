#include <chrono>

#include "test/mocks/http/mocks.h"
#include "test/mocks/local_info/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/cluster_info.h"
#include "test/test_common/utility.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "envoy/config/route/v3/route_components.pb.h"

#include "source/common/json/json_loader.h"
#include "source/common/network/address_impl.h"

#include "filters/api/envoy/extensions/filters/http/strong_local_ratelimit/v3/strong_local_ratelimit.pb.h"
#include "filters/api/envoy/extensions/filters/http/strong_local_ratelimit/v3/strong_local_ratelimit_log.pb.h"
#include "filters/source/extensions/filters/http/strong_local_ratelimit/strong_local_ratelimit.h"
#include "filters/source/extensions/filters/http/strong_local_ratelimit/config.h"

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

  // Stats::StatName statName() const override {
  //   stat_name_ = std::make_unique<Stats::StatNameManagedStorage>(name_, *symbol_table_);
  //   return stat_name_->statName();
  // }

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
namespace StrongLocalRateLimitFilter {

namespace v3 = envoy::extensions::filters::http::strong_local_ratelimit::v3;

static const std::string yaml_per_filter = R"(
  "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
  enable: true
  dryrun: false
  rules:
    - route_id: [0, 1]
      action:
        target: {}
        header:
          name: whoami
          present_match: true
        quotas:
          - duration: 1
            max_count: 5
        after_pass: NEXT_RULE
    - route_id: [1]
      action:
        target: {}
        header:
          name: whoami
          present_match: true
        quotas:
          - duration: 1
            max_count: 3
        after_pass: RETURN
  )";

// nodeId.网关名称.虚拟服务名称.集群名称:插件名称.插件实例ID
// nodeid1.gw1.vs1.cluster1:envoy.filters.http.acl.1.0.123456
struct NodeInfo {
  std::string nodeid;
  std::string gwid;
  std::string vsname;
  std::string cluster;
  std::string filtername;
};
static const NodeInfo g_node_Info = {
    "nodeid-1", "route-1", "vs-1", "cluster-1",
    "envoy.filters.http.strong-local-ratelimit.1.0.instance123213"};

static const std::string g_cluster_name_direct_ = "_direct_response";
static const std::string g_cluster_name_redirect_ = "_redirect";

class FilterTest : public testing::Test {
public:
  FilterTest() = default;

  void setup(const std::string& yaml_per_filter) {
    v3::StrongLocalRateLimitGlobal global_config;
    global_config_ = std::make_shared<FilterGlobalConfig>(global_config, dispatcher_, stats_);

    // 在单元测试中禁用配额的实例数量统计,因为server范围的scope等同于全局的，在单元测试中不好处理全局的scope(测试退出时报内存泄漏)
    Impl::Quota::setStats(nullptr);

    cluster_info_ptr_ = std::make_shared<NiceMock<Envoy::Upstream::MockClusterInfo>>();
    cluster_info_ = cluster_info_ptr_;

    (*context_.bootstrap_.mutable_node()->mutable_id()) = g_node_Info.nodeid;
    // set filter's name into metadata
    {
      ProtobufWkt::Struct metadata;
      auto& fields = *metadata.mutable_fields();
      fields["filter_name"].set_string_value(g_node_Info.filtername); // 单元测试在这里传递参数
      (*stream_info_.metadata_.mutable_filter_metadata())[HttpFilterNames::get().Composite]
          .MergeFrom(metadata);
    }

    // FilterConfig::timer_是全局的，在leak检测时还没析构
    testing::Mock::AllowLeak(FilterGlobalConfig::timer_.get());

    ON_CALL(context_, scope()).WillByDefault(ReturnRef(stats_));

    envoy::config::route::v3::RouteConfiguration routeConfiguration;
    *routeConfiguration.mutable_name() = g_node_Info.gwid;

    configImpl_ = std::make_shared<Router::ConfigImpl>(
        routeConfiguration, Router::OptionalHttpFilters(), context_,
        ProtobufMessage::getNullValidationVisitor(), true);

    if (!yaml_per_filter.empty()) {
      envoy::config::route::v3::VirtualHost virtual_host;
      virtual_host.mutable_name()->operator=(g_node_Info.vsname);

      Protobuf::Any config2;
      TestUtility::loadFromYaml(yaml_per_filter, config2);
      virtual_host.mutable_typed_per_filter_config()->insert(
          {"envoy.filters.http.strong-local-ratelimit.1.0", config2});
      virtual_host_ = std::make_unique<NiceMock<Router::MockVirtualHostImpl>>(
          virtual_host, context_, stats_, *configImpl_);
    } else {
      envoy::config::route::v3::VirtualHost virtual_host;
      virtual_host_ = std::make_unique<NiceMock<Router::MockVirtualHostImpl>>(
          virtual_host, context_, stats_, *configImpl_);
    }

    route_ = std::make_shared<NiceMock<Router::MockRoute>>();
    route_->route_entry_.cluster_name_ = g_node_Info.cluster;

    ON_CALL(route_->route_entry_, virtualHost()).WillByDefault(ReturnRef(*virtual_host_));
    ON_CALL(stream_info_, route()).WillByDefault(Return(route_));

    ON_CALL(decoder_callbacks_, streamInfo()).WillByDefault(ReturnRef(stream_info_));
    ON_CALL(decoder_callbacks_, dispatcher()).WillByDefault(ReturnRef(dispatcher_));
    ON_CALL(stream_info_, upstreamClusterInfo()).WillByDefault(Return(cluster_info_));

    filter_ = std::make_shared<Filter>(global_config_, context_);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
  }
  uint32_t calcCleanQuotasElapse() const { return FilterGlobalConfig::calcCleanQuotasElapse(); }

  StrongLocalRateLimitFilterFactory factory_;
  NiceMock<Server::Configuration::MockServerFactoryContext> context_;
  NiceMock<Stats::MockIsolatedStatsStore> stats_;
  testing::NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Event::MockDispatcher> dispatcher_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info_;
  std::shared_ptr<NiceMock<Router::MockRoute>> route_;

  std::shared_ptr<NiceMock<Envoy::Upstream::MockClusterInfo>> cluster_info_ptr_;
  absl::optional<Envoy::Upstream::ClusterInfoConstSharedPtr> cluster_info_;
  std::unique_ptr<NiceMock<Router::MockVirtualHostImpl>> virtual_host_;
  std::shared_ptr<FilterGlobalConfig> global_config_;
  std::shared_ptr<Filter> filter_;

  std::shared_ptr<Router::ConfigImpl> configImpl_;

  std::shared_ptr<NiceMock<Router::MyMockRoute>> myRoute_;

  // for key test
  void setupForKey(std::function<void()> setupRoute) {
    std::string yaml_per_filter = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
    enable: true
    dryrun: false
    rules:
      - route_id: [0, 1]
        upstream: fake_cluster
    )";
    v3::StrongLocalRateLimitGlobal global_config;
    global_config_ = std::make_shared<FilterGlobalConfig>(global_config, dispatcher_, stats_);

    cluster_info_ptr_ = std::make_shared<NiceMock<Envoy::Upstream::MockClusterInfo>>();
    cluster_info_ = cluster_info_ptr_;

    (*context_.bootstrap_.mutable_node()->mutable_id()) = g_node_Info.nodeid;
    // set filter's name into metadata
    {
      ProtobufWkt::Struct metadata;
      auto& fields = *metadata.mutable_fields();
      fields["filter_name"].set_string_value(g_node_Info.filtername); // 单元测试在这里传递参数
      (*stream_info_.metadata_.mutable_filter_metadata())[HttpFilterNames::get().Composite]
          .MergeFrom(metadata);
    }

    ON_CALL(context_, scope()).WillByDefault(ReturnRef(stats_));

    envoy::config::route::v3::RouteConfiguration routeConfiguration;
    *routeConfiguration.mutable_name() = g_node_Info.gwid;

    configImpl_ = std::make_shared<Router::ConfigImpl>(
        routeConfiguration, Router::OptionalHttpFilters(), context_,
        ProtobufMessage::getNullValidationVisitor(), true);

    envoy::config::route::v3::VirtualHost virtual_host;
    *virtual_host.mutable_name() = g_node_Info.vsname;

    Protobuf::Any config2;
    TestUtility::loadFromYaml(yaml_per_filter, config2);
    virtual_host.mutable_typed_per_filter_config()->insert(
        {"envoy.filters.http.strong-local-ratelimit.1.0", config2});
    virtual_host_ = std::make_unique<NiceMock<Router::MockVirtualHostImpl>>(virtual_host, context_,
                                                                            stats_, *configImpl_);
    setupRoute();

    ON_CALL(decoder_callbacks_, streamInfo()).WillByDefault(ReturnRef(stream_info_));
    ON_CALL(decoder_callbacks_, dispatcher()).WillByDefault(ReturnRef(dispatcher_));
    ON_CALL(stream_info_, upstreamClusterInfo()).WillByDefault(Return(cluster_info_));

    filter_ = std::make_shared<Filter>(global_config_, context_);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
  }
};

TEST_F(FilterTest, LogFormatKeyForRedirect) {
  auto setupRoute = [this]() {
    envoy::config::route::v3::Route routePb;
    routePb.mutable_redirect()->set_host_redirect("127.0.0.1");
    routePb.mutable_redirect()->set_port_redirect(4567);
    myRoute_ = std::make_shared<NiceMock<Router::MyMockRoute>>(
        *virtual_host_, routePb, Router::OptionalHttpFilters(), context_,
        ProtobufMessage::getNullValidationVisitor());
    myRoute_->route_entry_.cluster_name_ = g_node_Info.cluster;

    ON_CALL(myRoute_->route_entry_, virtualHost()).WillByDefault(ReturnRef(*virtual_host_));
    ON_CALL(*myRoute_, routeEntry()).WillByDefault(Return(nullptr));
    ON_CALL(stream_info_, route()).WillByDefault(Return(myRoute_));
  };
  setupForKey(setupRoute);

  auto key = filter_->getFilterAccessLogKey();
  EXPECT_EQ(fmt::format("[{}].[{}].[{}].[{}]:[{}]", g_node_Info.nodeid, g_node_Info.gwid, g_node_Info.vsname,
                        g_cluster_name_redirect_, g_node_Info.filtername),
            key);
}

TEST_F(FilterTest, LogFormatKeyForDirectResponse) {
  auto setupRoute = [this]() {
    envoy::config::route::v3::Route routePb;
    routePb.mutable_direct_response()->set_status(200);
    myRoute_ = std::make_shared<NiceMock<Router::MyMockRoute>>(
        *virtual_host_, routePb, Router::OptionalHttpFilters(), context_,
        ProtobufMessage::getNullValidationVisitor());
    myRoute_->route_entry_.cluster_name_ = g_node_Info.cluster;
    ON_CALL(myRoute_->route_entry_, virtualHost()).WillByDefault(ReturnRef(*virtual_host_));
    ON_CALL(stream_info_, route()).WillByDefault(Return(myRoute_));
    ON_CALL(*myRoute_, routeEntry()).WillByDefault(Return(nullptr));
  };
  setupForKey(setupRoute);

  auto key = filter_->getFilterAccessLogKey();
  EXPECT_EQ(fmt::format("[{}].[{}].[{}].[{}]:[{}]", g_node_Info.nodeid, g_node_Info.gwid, g_node_Info.vsname,
                        g_cluster_name_direct_, g_node_Info.filtername),
            key);
}

// 验证日志格式
TEST_F(FilterTest, LogFormatALL) {
  std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
    enable: true
    dryrun: false
    rules:
      - route_id: [0, 1]
        upstream: fake_cluster
        action:
          target: ALL
          quotas:
            - duration: 1
              max_count: 5
    )";
  setup(fmt::format(yaml_local));
  auto key = filter_->getFilterAccessLogKey();
  EXPECT_EQ(fmt::format("[{}].[{}].[{}].[{}]:[{}]", g_node_Info.nodeid, g_node_Info.gwid, g_node_Info.vsname,
                        g_node_Info.cluster, g_node_Info.filtername),
            key);
  auto headers = Http::TestRequestHeaderMapImpl();

  for (size_t i = 0; i < 5; i++) {
    ASSERT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
    ASSERT_EQ(i + 1, filter_->test_stat_of_Filter_.match);
    auto log_json = MessageUtil::getJsonStringFromMessageOrDie(filter_->getLog(), false, true);

    Envoy::Json::ObjectSharedPtr log_json_object = Envoy::Json::Factory::loadFromString(log_json);
    EXPECT_EQ(log_json_object->getInteger("act"), v3::StrongLocalRatelimitLog::ALLOW);
    EXPECT_EQ(log_json_object->getInteger("target"), v3::StrongLocalRatelimitLog::ALL);
  }
  ASSERT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
  ASSERT_EQ(6, filter_->test_stat_of_Filter_.match);

  auto log_json = MessageUtil::getJsonStringFromMessageOrDie(filter_->getLog(), false, true);
  Envoy::Json::ObjectSharedPtr log_json_object = Envoy::Json::Factory::loadFromString(log_json);

  EXPECT_EQ(log_json_object->getInteger("act"), v3::StrongLocalRatelimitLog::LIMIT);
  EXPECT_EQ(log_json_object->getInteger("target"), v3::StrongLocalRatelimitLog::ALL);
}

TEST_F(FilterTest, LogFormatIP) {
  auto yaml_local = R"(
      "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
      enable: true
      dryrun: false
      rules:
        src_ip: 
          address_prefix: 127.0.0.1
        action:
          quotas: 
            duration: 1
            max_count: 5
          target: IP
    )";
  setup(yaml_local); // v3::Action::IP todo
  auto key = filter_->getFilterAccessLogKey();
  EXPECT_EQ(fmt::format("[{}].[{}].[{}].[{}]:[{}]", g_node_Info.nodeid, g_node_Info.gwid, g_node_Info.vsname,
                        g_node_Info.cluster, g_node_Info.filtername),
            key);
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(
      std::make_unique<Network::Address::Ipv4Instance>("127.0.0.1", 5678));
  auto headers = Http::TestRequestHeaderMapImpl();
  for (size_t i = 0; i < 5; i++) {
    ASSERT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
    ASSERT_EQ(i + 1, filter_->test_stat_of_Filter_.match);
    auto log_json = MessageUtil::getJsonStringFromMessageOrDie(filter_->getLog(), false, true);
    Envoy::Json::ObjectSharedPtr log_json_object = Envoy::Json::Factory::loadFromString(log_json);

    EXPECT_EQ(log_json_object->getInteger("act"), v3::StrongLocalRatelimitLog::ALLOW);
    EXPECT_EQ(log_json_object->getInteger("target"), v3::StrongLocalRatelimitLog::ALL);
  }
  ASSERT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
  ASSERT_EQ(6, filter_->test_stat_of_Filter_.match);

  auto log_json = MessageUtil::getJsonStringFromMessageOrDie(filter_->getLog(), false, true);
  Envoy::Json::ObjectSharedPtr log_json_object = Envoy::Json::Factory::loadFromString(log_json);

  EXPECT_EQ(log_json_object->getInteger("act"), v3::StrongLocalRatelimitLog::LIMIT);
  EXPECT_EQ(log_json_object->getInteger("target"), v3::StrongLocalRatelimitLog::IP);
}

TEST_F(FilterTest, LogFormatHEADER) {
  std::string yaml_template = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
    enable: true
    dryrun: false
    rules:
      - route_id: {}
        upstream: {}
        src_ip:
          address_prefix: {}
          prefix_len: {}
        condition:
          - regex: {}
          - prefix: {}
            headers:
              - name: {}
                prefix_match: {}
            query_parameters:
              - name: {}
                value:
                  exact: {}
            methods: {}
        action:
          target: {}
          header:
            name: {}
            present_match: true
          quotas:
            - duration: {}
              max_count: {}
          after_pass: RETURN
    )";

  std::string yaml_local = fmt::format(yaml_template,
                                       "[6,7,8,9]",        // route_id
                                       "helloworld",       // upstream
                                       "192.168.1.1", 24,  // src_ip
                                       "^.*",              // condition: regex
                                       "/a",               // condition: prefix
                                       "whoami", "prefix", // condition: headers
                                       "p1", "p1_value",   // condition: query_parameters
                                       "[GET,POST]",       // condition: methods
                                       "2",                // action: targer
                                       "whoami",           // action: header
                                       1, 5               // action: quotas
  );

  std::string upstream_name = "helloworld";
  std::string ip = "192.168.1.88";
  {
    uint32_t enabled = 0;
    uint32_t match = 0;
    uint32_t ok = 0;

    setup(yaml_local);
    auto key = filter_->getFilterAccessLogKey();
    EXPECT_EQ(fmt::format("[{}].[{}].[{}].[{}]:[{}]", g_node_Info.nodeid, g_node_Info.gwid,
                          g_node_Info.vsname, g_node_Info.cluster, g_node_Info.filtername),
              key);
    Protobuf::Value v;
    v.set_number_value(8);
    Protobuf::Struct route_id;
    route_id.mutable_fields()->insert({"route_id", v});
    route_->metadata_.mutable_filter_metadata()->insert({"route_id", route_id});
    cluster_info_ptr_->name_ = upstream_name;
    stream_info_.downstream_connection_info_provider_->setRemoteAddress(
        std::make_unique<Network::Address::Ipv4Instance>(ip, 5678));
    {
      auto headers = Http::TestRequestHeaderMapImpl();
      headers.setMethod("get");
      headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "prefix_i am LiLei");
      headers.setCopy(Envoy::Http::LowerCaseString("whoami2"), "i am HanMeimei_suffix");
      headers.setPath("/aasdf?p1=p1_value&p2=p2_value");

      for (size_t i = 0; i < 5; i++) {
        ASSERT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
        ASSERT_EQ(++enabled, filter_->test_stat_of_Filter_.enable);
        ASSERT_EQ(++match, filter_->test_stat_of_Filter_.match);
        ASSERT_EQ(++ok, filter_->test_stat_of_Filter_.ok);

        auto log_json = MessageUtil::getJsonStringFromMessageOrDie(filter_->getLog(), false, true);
        Envoy::Json::ObjectSharedPtr log_json_object =
            Envoy::Json::Factory::loadFromString(log_json);

        EXPECT_EQ(log_json_object->getInteger("act"), v3::StrongLocalRatelimitLog::ALLOW);
        EXPECT_EQ(log_json_object->getInteger("target"), v3::StrongLocalRatelimitLog::ALL);
      }
      ASSERT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
      ASSERT_EQ(++enabled, filter_->test_stat_of_Filter_.enable);
      ASSERT_EQ(++match, filter_->test_stat_of_Filter_.match);
      ASSERT_EQ(ok, filter_->test_stat_of_Filter_.ok);

      ASSERT_EQ(6, filter_->test_stat_of_Filter_.match);

      auto log_json = MessageUtil::getJsonStringFromMessageOrDie(filter_->getLog(), false, true);
      Envoy::Json::ObjectSharedPtr log_json_object = Envoy::Json::Factory::loadFromString(log_json);

      EXPECT_EQ(log_json_object->getInteger("act"), v3::StrongLocalRatelimitLog::LIMIT);
      EXPECT_EQ(log_json_object->getInteger("target"), v3::StrongLocalRatelimitLog::HEADER);
    }
  }
}

// 验证没有VH配置时，不会执行
TEST_F(FilterTest, NoPerFilterConfigOverride) {
  setup("");
  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  EXPECT_EQ(0U, filter_->test_stat_of_Filter_.enable);
}

// 验证有VH配置时，会执行
TEST_F(FilterTest, PerFilterConfigOverride) {
  setup(fmt::format(yaml_per_filter, v3::Action::ALL, v3::Action::ALL));
  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
}

// 验证动态调整清理缓存频率是否正确
TEST_F(FilterTest, CalcCleanQuotasElapse) {
  setup(fmt::format(yaml_per_filter, v3::Action::HEADER, v3::Action::HEADER));
  cluster_info_ptr_->name_ = "";
  auto headers = Http::TestRequestHeaderMapImpl();

  // 清空所有流控项
  while(Impl::Action::quotasSize()){
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    Impl::Action::cleanQuotas(1000);
  }
  ASSERT_EQ(0, Impl::Action::quotasSize());

  EXPECT_EQ(calcCleanQuotasElapse(), LRU_CACHE_CLEAN_ELAPSE_MAX);

  // 逐个增加流控项，此时时间间隔应该逐渐减少
  uint32_t last_elapse = calcCleanQuotasElapse();
  for (size_t i = 0; i < Impl::Action::quotasMaxSize(); i++) {
    headers.setCopy(Envoy::Http::LowerCaseString("whoami"), std::to_string(i));
    ASSERT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
    uint32_t elapse = calcCleanQuotasElapse();
    if (elapse != last_elapse) {
      ASSERT_LT(elapse, last_elapse);
      last_elapse = elapse;
    }
  }
  EXPECT_EQ(calcCleanQuotasElapse(), LRU_CACHE_CLEAN_ELAPSE_MIN);

  // 逐个删除流控项,此时时间间隔应该逐渐增加
  last_elapse = calcCleanQuotasElapse();
  std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 等待流控项失效
  for (size_t i = 0; i < Impl::Action::quotasMaxSize(); i++) {
    Impl::Action::cleanQuotas(1);
    uint32_t elapse = calcCleanQuotasElapse();
    if (elapse != last_elapse) {
      ASSERT_GT(elapse, last_elapse);
      last_elapse = elapse;
    }
  }
  EXPECT_EQ(calcCleanQuotasElapse(), LRU_CACHE_CLEAN_ELAPSE_MAX);

  EXPECT_EQ(Impl::Action::quotasMaxSize(), filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(Impl::Action::quotasMaxSize(), filter_->test_stat_of_Filter_.match);
  EXPECT_EQ(Impl::Action::quotasMaxSize(), filter_->test_stat_of_Filter_.ok);
}

// 验证Enable未启用时，不会执行
TEST_F(FilterTest, Disable) {
  const std::string yaml_local = R"(
  "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
  enable: false
  dryrun: false
  rules:
    - route_id: [0, 1]
  )";
  setup(yaml_local);
  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  EXPECT_EQ(0U, filter_->test_stat_of_Filter_.enable);
}

// 验证规则匹配没有配置时，应该匹配失败
TEST_F(FilterTest, MatchRouteEmpty) {
  std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
    enable: true
    dryrun: false
    rules:
      - action:
          target: ALL
          quotas:
            - duration: 1
              max_count: 5
    )";
  setup(yaml_local);

  auto headers = Http::TestRequestHeaderMapImpl();
  filter_->decodeHeaders(headers, false);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(0U, filter_->test_stat_of_Filter_.match);
}

// 验证规则匹配是否正确
TEST_F(FilterTest, MatchRoute) {
  // route_id、upstream
  std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
    enable: true
    dryrun: false
    rules:
      - route_id: [6,7,8,9]
        upstream: helloworld
    )";
  setup(yaml_local);

  // 匹配
  Protobuf::Value v;
  v.set_number_value(8);

  Protobuf::Struct route_id;
  route_id.mutable_fields()->insert({"route_id", v});
  route_->metadata_.mutable_filter_metadata()->insert({"route_id", route_id});
  cluster_info_ptr_->name_ = "helloworld";
  auto headers = Http::TestRequestHeaderMapImpl();
  filter_->decodeHeaders(headers, false);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.match);

  // 不匹配的route_id
  v.set_number_value(5);
  route_id.mutable_fields()->at("route_id") = v;
  route_->metadata_.mutable_filter_metadata()->at("route_id") = route_id;
  cluster_info_ptr_->name_ = "helloworld";
  filter_->decodeHeaders(headers, false);
  EXPECT_EQ(2U, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.match);

  // 不匹配的upsteam
  v.set_number_value(8);
  route_id.mutable_fields()->at("route_id") = v;
  route_->metadata_.mutable_filter_metadata()->at("route_id") = route_id;
  cluster_info_ptr_->name_ = "helloworld111";
  filter_->decodeHeaders(headers, false);
  EXPECT_EQ(3U, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.match);

  // src_ip
  yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
    enable: true
    dryrun: false
    rules:
      - route_id: [6,7,8,9]
        upstream: helloworld
        src_ip:
          address_prefix: 192.168.1.1
          prefix_len: 24
    )";
  setup(yaml_local);
  v.set_number_value(8);
  route_id.mutable_fields()->at("route_id") = v;
  route_->metadata_.mutable_filter_metadata()->insert({"route_id", route_id});
  cluster_info_ptr_->name_ = "helloworld";

  // 匹配
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(
      std::make_unique<Network::Address::Ipv4Instance>("192.168.1.88", 5678));
  filter_->decodeHeaders(headers, false);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.match);

  // 不匹配
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(
      std::make_unique<Network::Address::Ipv4Instance>("192.168.2.88", 5678));
  filter_->decodeHeaders(headers, false);
  EXPECT_EQ(2U, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.match);

  // condition prefix
  yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
    enable: true
    dryrun: false
    rules:
      - route_id: [6,7,8,9]
        upstream: helloworld
        src_ip:
          address_prefix: 192.168.1.1
          prefix_len: 24
        condition:
          - prefix: /a
          - prefix: /b
          - prefix: /
    )";

  //  匹配
  setup(yaml_local);
  v.set_number_value(8);
  route_id.mutable_fields()->at("route_id") = v;
  route_->metadata_.mutable_filter_metadata()->insert({"route_id", route_id});
  cluster_info_ptr_->name_ = "helloworld";
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(
      std::make_unique<Network::Address::Ipv4Instance>("192.168.1.88", 5678));
  headers.setPath("/");
  filter_->decodeHeaders(headers, false);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.match);
  headers.setPath("/basdf");
  filter_->decodeHeaders(headers, false);
  EXPECT_EQ(2U, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(2U, filter_->test_stat_of_Filter_.match);

  // 不匹配
  setup(yaml_local);
  cluster_info_ptr_->name_ = "helloworld";
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(
      std::make_unique<Network::Address::Ipv4Instance>("192.168.1.88", 5678));
  headers.setPath("/hello/world");
  filter_->decodeHeaders(headers, false);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(0U, filter_->test_stat_of_Filter_.match);

  // condition exact
  yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
    enable: true
    dryrun: false
    rules:
      - route_id: [6,7,8,9]
        upstream: helloworld
        src_ip:
          address_prefix: 192.168.1.1
          prefix_len: 24
        condition:
          - exact: /a
          - exact: /b
          - exact: /
    )";

  //  匹配
  setup(yaml_local);
  v.set_number_value(8);
  route_id.mutable_fields()->at("route_id") = v;
  route_->metadata_.mutable_filter_metadata()->insert({"route_id", route_id});
  cluster_info_ptr_->name_ = "helloworld";
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(
      std::make_unique<Network::Address::Ipv4Instance>("192.168.1.88", 5678));
  headers.setPath("/");
  filter_->decodeHeaders(headers, false);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.match);
  headers.setPath("/b");
  filter_->decodeHeaders(headers, false);
  EXPECT_EQ(2U, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(2U, filter_->test_stat_of_Filter_.match);

  // 不匹配
  setup(yaml_local);
  cluster_info_ptr_->name_ = "helloworld";
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(
      std::make_unique<Network::Address::Ipv4Instance>("192.168.1.88", 5678));
  headers.setPath("/basdfasdf");
  filter_->decodeHeaders(headers, false);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(0U, filter_->test_stat_of_Filter_.match);

  // condition regex
  yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
    enable: true
    dryrun: false
    rules:
      - route_id: [6,7,8,9]
        upstream: helloworld
        src_ip:
          address_prefix: 192.168.1.1
          prefix_len: 24
        condition:
          - regex: /a
          - regex: .*/b/c/.*
          - regex: /
    )";

  //  匹配
  setup(yaml_local);
  v.set_number_value(8);
  route_id.mutable_fields()->at("route_id") = v;
  route_->metadata_.mutable_filter_metadata()->insert({"route_id", route_id});
  cluster_info_ptr_->name_ = "helloworld";
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(
      std::make_unique<Network::Address::Ipv4Instance>("192.168.1.88", 5678));
  headers.setPath("/a");
  filter_->decodeHeaders(headers, false);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.match);
  headers.setPath("/hi/b/c/asdfasdf/asdfasdf");
  filter_->decodeHeaders(headers, false);
  EXPECT_EQ(2U, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(2U, filter_->test_stat_of_Filter_.match);

  // 不匹配
  setup(yaml_local);
  cluster_info_ptr_->name_ = "helloworld";
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(
      std::make_unique<Network::Address::Ipv4Instance>("192.168.1.88", 5678));
  headers.setPath("/b/cd/");
  filter_->decodeHeaders(headers, false);
  EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(0U, filter_->test_stat_of_Filter_.match);

  // condition headers
  yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
    enable: true
    dryrun: false
    rules:
      - route_id: [6,7,8,9]
        upstream: helloworld
        src_ip:
          address_prefix: 192.168.1.1
          prefix_len: 24
        condition:
          - regex: .*/b/c/.*
          - prefix: /a
            headers:
              - name: whoami
                prefix_match: prefix
              - name: whoami2
                suffix_match: suffix 
    )";

  //  匹配
  setup(yaml_local);
  v.set_number_value(8);
  route_id.mutable_fields()->at("route_id") = v;
  route_->metadata_.mutable_filter_metadata()->insert({"route_id", route_id});
  cluster_info_ptr_->name_ = "helloworld";
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(
      std::make_unique<Network::Address::Ipv4Instance>("192.168.1.88", 5678));
  {
    auto headers = Http::TestRequestHeaderMapImpl();
    headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "prefix_i am LiLei");
    headers.setCopy(Envoy::Http::LowerCaseString("whoami2"), "i am HanMeimei_suffix");
    headers.setPath("/a");
    filter_->decodeHeaders(headers, false);
    EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
    EXPECT_EQ(1U, filter_->test_stat_of_Filter_.match);
  }

  // 不匹配
  setup(yaml_local);
  v.set_number_value(8);
  route_id.mutable_fields()->at("route_id") = v;
  route_->metadata_.mutable_filter_metadata()->insert({"route_id", route_id});
  cluster_info_ptr_->name_ = "helloworld";
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(
      std::make_unique<Network::Address::Ipv4Instance>("192.168.1.88", 5678));
  {
    auto headers = Http::TestRequestHeaderMapImpl();
    headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "prefix_i am LiLei");
    headers.setPath("/a");
    filter_->decodeHeaders(headers, false);
    EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
    EXPECT_EQ(0U, filter_->test_stat_of_Filter_.match);
  }

  {
    auto headers = Http::TestRequestHeaderMapImpl();
    headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "prefix_i am LiLei");
    headers.setCopy(Envoy::Http::LowerCaseString("whoami2"), "i am HanMeimei_suffix223123");
    headers.setPath("/a");
    filter_->decodeHeaders(headers, false);
    EXPECT_EQ(2U, filter_->test_stat_of_Filter_.enable);
    EXPECT_EQ(0U, filter_->test_stat_of_Filter_.match);
  }

  // condition QueryParameterMatcher
  yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
    enable: true
    dryrun: false
    rules:
      - route_id: [6,7,8,9]
        upstream: helloworld
        src_ip:
          address_prefix: 192.168.1.1
          prefix_len: 24
        condition:
          - regex: .*/b/c/.*
          - prefix: /a
            headers:
              - name: whoami
                prefix_match: prefix
              - name: whoami2
                suffix_match: suffix
            query_parameters:
              - name: p1
                value:
                  exact: p1_value
              - name: p2
                value:
                  exact: p2_value
    )";

  //  匹配
  setup(yaml_local);
  v.set_number_value(8);
  route_id.mutable_fields()->at("route_id") = v;
  route_->metadata_.mutable_filter_metadata()->insert({"route_id", route_id});
  cluster_info_ptr_->name_ = "helloworld";
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(
      std::make_unique<Network::Address::Ipv4Instance>("192.168.1.88", 5678));
  {
    auto headers = Http::TestRequestHeaderMapImpl();
    headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "prefix_i am LiLei");
    headers.setCopy(Envoy::Http::LowerCaseString("whoami2"), "i am HanMeimei_suffix");
    headers.setPath("/aasdf?p1=p1_value&p2=p2_value");
    filter_->decodeHeaders(headers, false);
    EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
    EXPECT_EQ(1U, filter_->test_stat_of_Filter_.match);
  }

  {
    auto headers = Http::TestRequestHeaderMapImpl();
    headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "prefix_i am LiLei");
    headers.setCopy(Envoy::Http::LowerCaseString("whoami2"), "i am HanMeimei_suffix");
    headers.setPath("/aasdasdfasdff?pa=pa_value&pb=pb_value&p1=p1_value&p2=p2_value");
    filter_->decodeHeaders(headers, false);
    EXPECT_EQ(2U, filter_->test_stat_of_Filter_.enable);
    EXPECT_EQ(2U, filter_->test_stat_of_Filter_.match);
  }

  // 不匹配
  setup(yaml_local);
  v.set_number_value(8);
  route_id.mutable_fields()->at("route_id") = v;
  route_->metadata_.mutable_filter_metadata()->insert({"route_id", route_id});
  cluster_info_ptr_->name_ = "helloworld";
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(
      std::make_unique<Network::Address::Ipv4Instance>("192.168.1.88", 5678));
  {
    auto headers = Http::TestRequestHeaderMapImpl();
    headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "prefix_i am LiLei");
    headers.setCopy(Envoy::Http::LowerCaseString("whoami2"), "i am HanMeimei_suffix");
    headers.setPath("/aasdf?p1=p1_value");
    filter_->decodeHeaders(headers, false);
    EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
    EXPECT_EQ(0U, filter_->test_stat_of_Filter_.match);
  }

  {
    auto headers = Http::TestRequestHeaderMapImpl();
    headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "prefix_i am LiLei");
    headers.setCopy(Envoy::Http::LowerCaseString("whoami2"), "i am HanMeimei_suffix");
    headers.setPath("/aasdf?p1=p1_value&p2=p2_value123123");
    filter_->decodeHeaders(headers, false);
    EXPECT_EQ(2U, filter_->test_stat_of_Filter_.enable);
    EXPECT_EQ(0U, filter_->test_stat_of_Filter_.match);
  }

  // condition Methods
  yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
    enable: true
    dryrun: false
    rules:
      - route_id: [6,7,8,9]
        upstream: helloworld
        src_ip:
          address_prefix: 192.168.1.1
          prefix_len: 24
        condition:
          - regex: .*/b/c/.*
          - prefix: /a
            headers:
              - name: whoami
                prefix_match: prefix
              - name: whoami2
                suffix_match: suffix
            query_parameters:
              - name: p1
                value:
                  exact: p1_value
              - name: p2
                value:
                  exact: p2_value
            methods: [GET,POST]
    )";

  //  匹配
  setup(yaml_local);
  v.set_number_value(8);
  route_id.mutable_fields()->at("route_id") = v;
  route_->metadata_.mutable_filter_metadata()->insert({"route_id", route_id});
  cluster_info_ptr_->name_ = "helloworld";
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(
      std::make_unique<Network::Address::Ipv4Instance>("192.168.1.88", 5678));
  {
    auto headers = Http::TestRequestHeaderMapImpl();
    headers.setMethod("get");
    headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "prefix_i am LiLei");
    headers.setCopy(Envoy::Http::LowerCaseString("whoami2"), "i am HanMeimei_suffix");
    headers.setPath("/aasdf?p1=p1_value&p2=p2_value");
    filter_->decodeHeaders(headers, false);
    EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
    EXPECT_EQ(1U, filter_->test_stat_of_Filter_.match);
  }

  {
    auto headers = Http::TestRequestHeaderMapImpl();
    headers.setMethod("POST");
    headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "prefix_i am LiLei");
    headers.setCopy(Envoy::Http::LowerCaseString("whoami2"), "i am HanMeimei_suffix");
    headers.setPath("/aasdf?p1=p1_value&p2=p2_value");
    filter_->decodeHeaders(headers, false);
    EXPECT_EQ(2U, filter_->test_stat_of_Filter_.enable);
    EXPECT_EQ(2U, filter_->test_stat_of_Filter_.match);
  }

  // 不匹配
  setup(yaml_local);
  v.set_number_value(8);
  route_id.mutable_fields()->at("route_id") = v;
  route_->metadata_.mutable_filter_metadata()->insert({"route_id", route_id});
  cluster_info_ptr_->name_ = "helloworld";
  stream_info_.downstream_connection_info_provider_->setRemoteAddress(
      std::make_unique<Network::Address::Ipv4Instance>("192.168.1.88", 5678));
  {
    auto headers = Http::TestRequestHeaderMapImpl();
    headers.setMethod("head");
    headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "prefix_i am LiLei");
    headers.setCopy(Envoy::Http::LowerCaseString("whoami2"), "i am HanMeimei_suffix");
    headers.setPath("/aasdf?p1=p1_value&p2=p2_value");
    filter_->decodeHeaders(headers, false);
    EXPECT_EQ(1U, filter_->test_stat_of_Filter_.enable);
    EXPECT_EQ(0U, filter_->test_stat_of_Filter_.match);
  }

  {
    auto headers = Http::TestRequestHeaderMapImpl();
    headers.setMethod("OPTION");
    headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "prefix_i am LiLei");
    headers.setCopy(Envoy::Http::LowerCaseString("whoami2"), "i am HanMeimei_suffix");
    headers.setPath("/aasdf?p1=p1_value&p2=p2_value");
    filter_->decodeHeaders(headers, false);
    EXPECT_EQ(2U, filter_->test_stat_of_Filter_.enable);
    EXPECT_EQ(0U, filter_->test_stat_of_Filter_.match);
  }
}

// 验证被限速时，HTTP头被添加
TEST_F(FilterTest, AddHeaders) {
  std::string yaml_local = R"(
  "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
  enable: true
  dryrun: false
  rules:
    - route_id: [0, 1]
      upstream: fake_cluster
      action:
        target: ALL
        add_headers: {}
        quotas:
          - duration: 1
            max_count: 5
        after_pass: NEXT_RULE
  )";

  setup(fmt::format(yaml_local, true));

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::TooManyRequests, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)> modify_headers,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::TooManyRequests, code);
        EXPECT_EQ("", body);

        Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
        modify_headers(response_headers);
        EXPECT_TRUE(response_headers.has(Http::LowerCaseString("X-RateLimit-Duration")));
        EXPECT_TRUE(response_headers.has(Http::LowerCaseString("X-RateLimit-Remaining")));

        EXPECT_EQ(grpc_status, absl::nullopt);
        EXPECT_EQ(details, "");
      }));

  auto headers = Http::TestRequestHeaderMapImpl();
  for (size_t i = 0; i < 5; i++) {
    ASSERT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
    ASSERT_EQ(i + 1, filter_->test_stat_of_Filter_.enable);
    ASSERT_EQ(i + 1, filter_->test_stat_of_Filter_.match);
    ASSERT_EQ(i + 1, filter_->test_stat_of_Filter_.ok);
  }
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
  EXPECT_EQ(6, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(6, filter_->test_stat_of_Filter_.match);
  EXPECT_EQ(5, filter_->test_stat_of_Filter_.ok);
}

// 验证空转，是否正确
TEST_F(FilterTest, Dryrun) {
  std::string yaml_local = R"(
  "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
  enable: true
  dryrun: true
  rules:
    - route_id: [0, 1]
      upstream: fake_cluster
      action:
        target: ALL
        add_headers: {}
        quotas:
          - duration: 1
            max_count: 5
        after_pass: NEXT_RULE
  )";

  setup(fmt::format(yaml_local, true));

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::TooManyRequests, _, _, _, _)).Times(0);

  auto headers = Http::TestRequestHeaderMapImpl();
  for (size_t i = 0; i < 5; i++) {
    ASSERT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
    ASSERT_EQ(i + 1, filter_->test_stat_of_Filter_.enable);
    ASSERT_EQ(i + 1, filter_->test_stat_of_Filter_.match);
    ASSERT_EQ(i + 1, filter_->test_stat_of_Filter_.ok);
  }
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  EXPECT_EQ(6, filter_->test_stat_of_Filter_.enable);
  EXPECT_EQ(6, filter_->test_stat_of_Filter_.match);
  EXPECT_EQ(6, filter_->test_stat_of_Filter_.ok);

  EXPECT_TRUE(headers.has(Http::LowerCaseString("X-RateLimit-Limit")));
}

// 验证修改规则后，流控项是否更新
TEST_F(FilterTest, ModifyRule) {
  // 清空所有流控项
  while(Impl::Action::quotasSize()){
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    Impl::Action::cleanQuotas(1000);
  }
  ASSERT_EQ(0, Impl::Action::quotasSize());

  std::string yaml_template = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_local_ratelimit.v3.StrongLocalRateLimitRoute
    enable: true
    dryrun: false
    rules:
      - route_id: {}
        upstream: {}
        src_ip:
          address_prefix: {}
          prefix_len: {}
        condition:
          - regex: {}
          - prefix: {}
            headers:
              - name: {}
                prefix_match: {}
            query_parameters:
              - name: {}
                value:
                  exact: {}
            methods: {}
        action:
          target: {}
          header:
            name: {}
            present_match: true
          quotas:
            - duration: {}
              max_count: {}
          after_pass: RETURN
    )";

  uint32_t quota_count = 0;

  std::string yaml_local = fmt::format(yaml_template,
                                       "[6,7,8,9]",        // route_id
                                       "helloworld",       // upstream
                                       "192.168.1.1", 24,  // src_ip
                                       "^.*",              // condition: regex
                                       "/a",               // condition: prefix
                                       "whoami", "prefix", // condition: headers
                                       "p1", "p1_value",   // condition: query_parameters
                                       "[GET,POST]",       // condition: methods
                                       "2",                // action: targer
                                       "whoami",           // action: header
                                       60, 5               // action: quotas
  );

  std::string upstream_name = "helloworld";
  std::string ip = "192.168.1.88";
  auto func = [&]() {
    uint32_t enabled = 0;
    uint32_t match = 0;
    uint32_t ok = 0;

    setup(yaml_local);
    Protobuf::Value v;
    v.set_number_value(8);
    Protobuf::Struct route_id;
    route_id.mutable_fields()->insert({"route_id", v});
    route_->metadata_.mutable_filter_metadata()->insert({"route_id", route_id});
    cluster_info_ptr_->name_ = upstream_name;
    stream_info_.downstream_connection_info_provider_->setRemoteAddress(
        std::make_unique<Network::Address::Ipv4Instance>(ip, 5678));
    {
      auto headers = Http::TestRequestHeaderMapImpl();
      headers.setMethod("get");
      headers.setCopy(Envoy::Http::LowerCaseString("whoami"), "prefix_i am LiLei");
      headers.setCopy(Envoy::Http::LowerCaseString("whoami2"), "i am HanMeimei_suffix");
      headers.setPath("/aasdf?p1=p1_value&p2=p2_value");

      for (size_t i = 0; i < 5; i++) {
        ASSERT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
        ASSERT_EQ(++enabled, filter_->test_stat_of_Filter_.enable);
        ASSERT_EQ(++match, filter_->test_stat_of_Filter_.match);
        ASSERT_EQ(++ok, filter_->test_stat_of_Filter_.ok);
      }
      ASSERT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
      ASSERT_EQ(++enabled, filter_->test_stat_of_Filter_.enable);
      ASSERT_EQ(++match, filter_->test_stat_of_Filter_.match);
      ASSERT_EQ(ok, filter_->test_stat_of_Filter_.ok);
    }

    EXPECT_EQ(++quota_count, Impl::Action::quotasSize());
  };
  func();

  // 修改route_id，验证流控项应该要更新
  yaml_local = fmt::format(yaml_template,
                           "[7,8,9]",          // route_id
                           "helloworld",       // upstream
                           "192.168.1.1", 24,  // src_ip
                           "^.*",              // condition: regex
                           "/a",               // condition: prefix
                           "whoami", "prefix", // condition: headers
                           "p1", "p1_value",   // condition: query_parameters
                           "[GET,POST]",       // condition: methods
                           "2",                // action: targer
                           "whoami",           // action: header
                           60, 5               // action: quotas
  );
  func();

  // 修改upstream，验证流控项应该要更新
  yaml_local = fmt::format(yaml_template,
                           "[7,8,9]",          // route_id
                           "helloworld2",      // upstream
                           "192.168.1.1", 24,  // src_ip
                           "^.*",              // condition: regex
                           "/a",               // condition: prefix
                           "whoami", "prefix", // condition: headers
                           "p1", "p1_value",   // condition: query_parameters
                           "[GET,POST]",       // condition: methods
                           "2",                // action: targer
                           "whoami",           // action: header
                           60, 5               // action: quotas
  );
  upstream_name = "helloworld2";
  func();

  // 修改src_ip，验证流控项应该要更新
  yaml_local = fmt::format(yaml_template,
                           "[7,8,9]",          // route_id
                           "helloworld2",      // upstream
                           "192.168.2.1", 24,  // src_ip
                           "^.*",              // condition: regex
                           "/a",               // condition: prefix
                           "whoami", "prefix", // condition: headers
                           "p1", "p1_value",   // condition: query_parameters
                           "[GET,POST]",       // condition: methods
                           "2",                // action: targer
                           "whoami",           // action: header
                           60, 5               // action: quotas
  );
  ip = "192.168.2.88";
  func();

  // 修改condition: regex，验证流控项应该要更新
  yaml_local = fmt::format(yaml_template,
                           "[7,8,9]",          // route_id
                           "helloworld2",      // upstream
                           "192.168.2.1", 24,  // src_ip
                           "^.*&",             // condition: regex
                           "/a",               // condition: prefix
                           "whoami", "prefix", // condition: headers
                           "p1", "p1_value",   // condition: query_parameters
                           "[GET,POST]",       // condition: methods
                           "2",                // action: targer
                           "whoami",           // action: header
                           60, 5               // action: quotas
  );
  func();

  // 修改condition: prefix，验证流控项应该要更新
  yaml_local = fmt::format(yaml_template,
                           "[7,8,9]",          // route_id
                           "helloworld2",      // upstream
                           "192.168.2.1", 24,  // src_ip
                           "^.*&",             // condition: regex
                           "/aa",              // condition: prefix
                           "whoami", "prefix", // condition: headers
                           "p1", "p1_value",   // condition: query_parameters
                           "[GET,POST]",       // condition: methods
                           "2",                // action: targer
                           "whoami",           // action: header
                           60, 5               // action: quotas
  );
  func();

  // 修改condition: headers，验证流控项应该要更新
  yaml_local = fmt::format(yaml_template,
                           "[7,8,9]",         // route_id
                           "helloworld2",     // upstream
                           "192.168.2.1", 24, // src_ip
                           "^.*&",            // condition: regex
                           "/aa",             // condition: prefix
                           "whoami", "pref",  // condition: headers
                           "p1", "p1_value",  // condition: query_parameters
                           "[GET,POST]",      // condition: methods
                           "2",               // action: targer
                           "whoami",          // action: header
                           60, 5              // action: quotas
  );
  func();

  // 修改condition: query_parameters，验证流控项应该要更新
  yaml_local = fmt::format(yaml_template,
                           "[7,8,9]",         // route_id
                           "helloworld2",     // upstream
                           "192.168.2.1", 24, // src_ip
                           "^.*&",            // condition: regex
                           "/aa",             // condition: prefix
                           "whoami", "pref",  // condition: headers
                           "p2", "p2_value",  // condition: query_parameters
                           "[GET,POST]",      // condition: methods
                           "2",               // action: targer
                           "whoami",          // action: header
                           60, 5              // action: quotas
  );
  func();

  // 修改condition: methods，验证流控项应该要更新
  yaml_local = fmt::format(yaml_template,
                           "[7,8,9]",         // route_id
                           "helloworld2",     // upstream
                           "192.168.2.1", 24, // src_ip
                           "^.*&",            // condition: regex
                           "/aa",             // condition: prefix
                           "whoami", "pref",  // condition: headers
                           "p2", "p2_value",  // condition: query_parameters
                           "[GET,POST,HEAD]", // condition: methods
                           "2",               // action: targer
                           "whoami",          // action: header
                           60, 5              // action: quotas
  );
  func();

  // 修改action: targer，验证流控项应该要更新
  yaml_local = fmt::format(yaml_template,
                           "[7,8,9]",         // route_id
                           "helloworld2",     // upstream
                           "192.168.2.1", 24, // src_ip
                           "^.*&",            // condition: regex
                           "/aa",             // condition: prefix
                           "whoami", "pref",  // condition: headers
                           "p2", "p2_value",  // condition: query_parameters
                           "[GET,POST,HEAD]", // condition: methods
                           "1",               // action: targer
                           "whoami",          // action: header
                           60, 5              // action: quotas
  );
  func();

  // 修改action: header，验证流控项应该要更新
  yaml_local = fmt::format(yaml_template,
                           "[7,8,9]",         // route_id
                           "helloworld2",     // upstream
                           "192.168.2.1", 24, // src_ip
                           "^.*&",            // condition: regex
                           "/aa",             // condition: prefix
                           "whoami", "pref",  // condition: headers
                           "p2", "p2_value",  // condition: query_parameters
                           "[GET,POST,HEAD]", // condition: methods
                           "2",               // action: targer
                           "whoami2",         // action: header
                           60, 5              // action: quotas
  );
  func();

  // 修改action: quotas，验证流控项应该要更新
  yaml_local = fmt::format(yaml_template,
                           "[7,8,9]",         // route_id
                           "helloworld2",     // upstream
                           "192.168.2.1", 24, // src_ip
                           "^.*&",            // condition: regex
                           "/aa",             // condition: prefix
                           "whoami", "pref",  // condition: headers
                           "p2", "p2_value",  // condition: query_parameters
                           "[GET,POST,HEAD]", // condition: methods
                           "2",               // action: targer
                           "whoami2",         // action: header
                           59, 5              // action: quotas
  );
  func();

  // 修改action: quotas，验证流控项应该要更新
  yaml_local = fmt::format(yaml_template,
                           "[7,8,9]",         // route_id
                           "helloworld2",     // upstream
                           "192.168.2.1", 24, // src_ip
                           "^.*&",            // condition: regex
                           "/aa",             // condition: prefix
                           "whoami", "pref",  // condition: headers
                           "p2", "p2_value",  // condition: query_parameters
                           "[GET,POST,HEAD]", // condition: methods
                           "2",               // action: targer
                           "whoami2",         // action: header
                           30, 5              // action: quotas
  );
  func();
}

} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
