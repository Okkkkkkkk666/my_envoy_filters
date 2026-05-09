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

#include "filters/api/envoy/extensions/filters/http/common/ratelimit/v3/ratelimit.pb.h"
#include "filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3/strong_global_ratelimit.pb.h"

#include "filters/source/extensions/filters/http/strong_global_ratelimit/config.h"
#include "filters/source/extensions/filters/http/strong_global_ratelimit/strong_global_ratelimit.h"

#include "filters/test/extensions/filters/http/common/ratelimit/mock_ratelimit_client.h"

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
namespace StrongGlobalRatelimit {

namespace v3 = envoy::extensions::filters::http::strong_global_ratelimit::v3;
namespace ratelimit_v3 = envoy::extensions::filters::http::common::ratelimit::v3;

// nodeId.网关名称.虚拟服务名称.集群名称:插件名称.插件实例ID
// nodeid1.gw1.vs1.cluster1:envoy.filters.http.acl.1.0.123456
struct NodeInfo {
  std::string nodeid;
  std::string gwid;
  std::string vsname;
  std::string cluster;
  std::string filtername;
};
static const NodeInfo g_node_Info = {"nodeid-1", "route-1", "vs-8", "cluster-1",
                                     "envoy.filters.http.strong-global-ratelimit.1.0"};

class FilterTest : public testing::Test {
public:
  FilterTest() = default;
  void setAddressToReturn(const std::string& address) {
    downstream_connection_info_provider_->setRemoteAddress(Network::Utility::resolveUrl(address));
  }

  void setup(const std::string& yaml_per_filter) {
    v3::StrongGlobalRateLimitGlobal global_config;
    global_config_ = std::make_shared<StrongGlobalRateLimitFilterGlobalConfig>(global_config);

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
    ON_CALL(context, scope()).WillByDefault(ReturnRef(stats_));
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
          {"envoy.filters.http.strong-global-ratelimit.1.0", config2});
      virtual_host_ = std::make_unique<NiceMock<Router::MockVirtualHostImpl>>(
          virtual_host, context_, stats_, *configImpl_);
    } else {
      envoy::config::route::v3::VirtualHost virtual_host;
      virtual_host_ = std::make_unique<NiceMock<Router::MockVirtualHostImpl>>(
          virtual_host, context_, stats_, *configImpl_);
    }

    route_ = std::make_shared<NiceMock<Router::MockRoute>>();
    route_->route_entry_.cluster_name_ = g_node_Info.cluster;
    Protobuf::Value v;
    v.set_number_value(1234);
    Protobuf::Struct route_id;
    route_id.mutable_fields()->insert({"route_id", v});
    route_->metadata_.mutable_filter_metadata()->insert({"route_id", route_id});
    downstream_connection_info_provider_ =
        std::make_shared<Network::ConnectionInfoSetterImpl>(nullptr, nullptr);

    ON_CALL(route_->route_entry_, virtualHost()).WillByDefault(ReturnRef(*virtual_host_));
    ON_CALL(stream_info_, route()).WillByDefault(Return(route_));
    ON_CALL(stream_info_, downstreamAddressProvider())
        .WillByDefault(ReturnRef(*downstream_connection_info_provider_));
    ON_CALL(stream_info_, upstreamClusterInfo()).WillByDefault(Return(cluster_info_));
    ON_CALL(decoder_callbacks_, activeSpan()).WillByDefault(ReturnRef(span_));
    ON_CALL(decoder_callbacks_, streamInfo()).WillByDefault(ReturnRef(stream_info_));
    ON_CALL(decoder_callbacks_, dispatcher()).WillByDefault(ReturnRef(dispatcher_));

    const std::chrono::milliseconds timeout =
        std::chrono::milliseconds(PROTOBUF_GET_MS_OR_DEFAULT(global_config.grpc(), timeout, 20));
    client_ = new Filters::Common::RatelimitClient::MockClient();

    filter_ = std::make_shared<StrongGlobalRateLimitFilter>(
        global_config_, std::move(Filters::Common::RatelimitClient::ClientPtr{client_}),
        context.getServerFactoryContext());
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
  }

  Filters::Common::RatelimitClient::MockClient* client_;
  StrongGlobalRatelimitFilterFactory factory_;
  NiceMock<Server::Configuration::MockServerFactoryContext> context_;
  NiceMock<Stats::MockIsolatedStatsStore> stats_;
  testing::NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Event::MockDispatcher> dispatcher_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info_;
  std::shared_ptr<NiceMock<Router::MockRoute>> route_;
  NiceMock<Tracing::MockSpan> span_;
  NiceMock<Server::Configuration::MockFactoryContext> context;
  std::shared_ptr<NiceMock<Envoy::Upstream::MockClusterInfo>> cluster_info_ptr_;
  absl::optional<Envoy::Upstream::ClusterInfoConstSharedPtr> cluster_info_;
  std::unique_ptr<NiceMock<Router::MockVirtualHostImpl>> virtual_host_;
  std::shared_ptr<StrongGlobalRateLimitFilterGlobalConfig> global_config_;
  std::shared_ptr<StrongGlobalRateLimitFilter> filter_;
  std::shared_ptr<Router::ConfigImpl> configImpl_;
  Filters::Common::RatelimitClient::LimitRequestCallbacks* request_callbacks_{};
  Grpc::MockAsyncClient* async_client_;
  NiceMock<Grpc::MockAsyncRequest> async_request_;
  std::shared_ptr<Network::ConnectionInfoSetterImpl> downstream_connection_info_provider_;
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
  "@type": type.googleapis.com/envoy.extensions.filters.http.strong_global_ratelimit.v3.StrongGlobalRateLimitRoute
  enable: false
  dryrun: false
  rules:
    - route_id: [0, 1]
  )";
  setup(yaml_local);
  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
}

// 验证匹配的IpWhitelist
TEST_F(FilterTest, MatchIpWhitelist) {
  const std::string yaml = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_global_ratelimit.v3.StrongGlobalRateLimitRoute
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
  setup(yaml);
  setAddressToReturn("tcp://192.192.100.1:80");
  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
}

// 验证没有匹配规则时，应该匹配失败并放行
TEST_F(FilterTest, MatchRuleEmpty) {
  const std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_global_ratelimit.v3.StrongGlobalRateLimitRoute
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
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
}

// 验证配额为0
TEST_F(FilterTest, MatchZeroQuota) {
  const std::string yaml = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_global_ratelimit.v3.StrongGlobalRateLimitRoute
    enable: true
    dryrun: true
    rules:
      - ip_invert: false
        route_id: 1234
        upstream: helloworld
        action:
          after_pass: NEXT_RULE
          target: ALL
          quotas: 
            - duration: 0
              max_count: 0
    )";
  setup(yaml);
  cluster_info_ptr_->name_ = "helloworld";
  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
}

// 验证规则匹配失败
TEST_F(FilterTest, MatchRuleFailure) {
  const std::string yaml = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_global_ratelimit.v3.StrongGlobalRateLimitRoute
    enable: true
    dryrun: false
    rules:
      - ip_invert: false
        route_id: [8,9]
        upstream: helloworld
        action:
          after_pass: NEXT_RULE
          target: ALL
          quotas: 
            - duration: 5
              max_count: 1
    )";
  setup(yaml);
  cluster_info_ptr_->name_ = "helloworld";
  auto headers = Http::TestRequestHeaderMapImpl();
  Buffer::OwnedImpl data_;
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data_, false));
}

// 验证规则匹配成功
TEST_F(FilterTest, MatchRuleSuccess) {
  const std::string yaml = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_global_ratelimit.v3.StrongGlobalRateLimitRoute
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
  EXPECT_CALL(*client_, limit(_, _, _, _)).Times(1);
  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
}

// 验证未超额
TEST_F(FilterTest, GrpcSuccess) {
  const std::string yaml = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_global_ratelimit.v3.StrongGlobalRateLimitRoute
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
  auto trailers = Http::TestRequestTrailerMapImpl();
  Buffer::OwnedImpl data;
  EXPECT_CALL(*client_, limit(_, _, _, _)).Times(1);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
  EXPECT_EQ(Http::FilterTrailersStatus::StopIteration, filter_->decodeTrailers(trailers));
  EXPECT_CALL(*client_, cancel());
  std::unique_ptr<ratelimit_v3::RateLimitResponse> response =
      std::unique_ptr<ratelimit_v3::RateLimitResponse>();
  filter_->onDestroy();
  EXPECT_CALL(decoder_callbacks_, continueDecoding());
  filter_->complete(Filters::Common::RatelimitClient::LimitStatus::OK, std::move(response));
  filter_->onStreamComplete();
}

// 验证超额且空转
TEST_F(FilterTest, OverLimitAndDryrun) {
  const std::string yaml = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_global_ratelimit.v3.StrongGlobalRateLimitRoute
    enable: true
    dryrun: true
    )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  EXPECT_CALL(decoder_callbacks_, continueDecoding());
  std::unique_ptr<ratelimit_v3::RateLimitResponse> response =
      std::make_unique<ratelimit_v3::RateLimitResponse>();
  filter_->complete(Filters::Common::RatelimitClient::LimitStatus::OverLimit, std::move(response));
}

// 验证超额且拒绝
TEST_F(FilterTest, OverLimitAndRefuse) {
  const std::string yaml = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_global_ratelimit.v3.StrongGlobalRateLimitRoute
    enable: true
    dryrun: false
    )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::TooManyRequests, _, _, _, _));
  std::unique_ptr<ratelimit_v3::RateLimitResponse> response =
      std::make_unique<ratelimit_v3::RateLimitResponse>();
  filter_->complete(Filters::Common::RatelimitClient::LimitStatus::OverLimit, std::move(response));
}

// 验证grpc请求Error
TEST_F(FilterTest, GrpcError) {
  const std::string yaml = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.strong_global_ratelimit.v3.StrongGlobalRateLimitRoute
    enable: true
    dryrun: false
    )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::InternalServerError, _, _, _, _));
  std::unique_ptr<ratelimit_v3::RateLimitResponse> response =
      std::make_unique<ratelimit_v3::RateLimitResponse>();
  filter_->complete(Filters::Common::RatelimitClient::LimitStatus::Error, std::move(response));
}

} // namespace StrongGlobalRatelimit
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy