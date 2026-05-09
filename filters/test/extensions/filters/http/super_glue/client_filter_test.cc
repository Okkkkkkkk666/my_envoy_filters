#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test/mocks/server/mocks.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/server/instance.h"
#include "test/mocks/api/mocks.h"
#include "test/mocks/tracing/mocks.h"
#include "test/test_common/utility.h"
#include "test/test_common/environment.h"

#include "source/common/http/message_impl.h"
#include "source/common/network/address_impl.h"
#include "filters/api/envoy/extensions/filters/http/super_glue/v3/super_glue_client.pb.h"
#include "filters/source/extensions/filters/http/super_glue/super_glue_client.h"
#include "filters/source/extensions/filters/http/super_glue/client_config.h"

using namespace testing;
using envoy::type::v3::FractionalPercent;

class FractionalPercentMatcher : public MatcherInterface<const FractionalPercent&> {
public:
  explicit FractionalPercentMatcher(const FractionalPercent& expected) : expected_(expected) {}

  bool MatchAndExplain(const FractionalPercent& actual, MatchResultListener*) const override {

    // 返回 true 表示匹配成功，返回 false 表示匹配失败
    return actual.numerator() == expected_.numerator() &&
           actual.denominator() == expected_.denominator();
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "matches the expected FractionalPercent";
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "does not match the expected FractionalPercent";
  }

private:
  const FractionalPercent& expected_;
};
inline Matcher<const FractionalPercent&> FractionalPercentEq(const FractionalPercent& expected) {
  return MakeMatcher(new FractionalPercentMatcher(expected));
}

class MockInstance : public Envoy::Network::Address::Instance {
public:
  MockInstance() {}
  ~MockInstance() = default;
  MOCK_METHOD(absl::string_view, asStringView, (), (const));
  MOCK_METHOD(const std::string&, asString, (), (const));
  MOCK_METHOD(const std::string&, ja3Hash, (), (const));
  MOCK_METHOD(bool, IsEqual, (const Instance& rhs), (const));
  MOCK_METHOD(const std::string&, logicalName, (), (const));
  MOCK_METHOD(const Envoy::Network::Address::Ip*, ip, (), (const));
  MOCK_METHOD(const Envoy::Network::Address::Pipe*, pipe, (), (const));
  MOCK_METHOD(const Envoy::Network::Address::EnvoyInternalAddress*, envoyInternalAddress, (),
              (const));
  MOCK_METHOD(const sockaddr*, sockAddr, (), (const));
  MOCK_METHOD(socklen_t, sockAddrLen, (), (const));
  MOCK_METHOD(Envoy::Network::Address::Type, type, (), (const));
  MOCK_METHOD(Envoy::Network::SocketInterface&, socketInterface, (), (const));
  bool operator==(const Instance& rhs) const { return IsEqual(rhs); }
};

class MockVirtualHostImpl : public Envoy::Router::VirtualHostImpl {
public:
  MockVirtualHostImpl(const envoy::config::route::v3::VirtualHost& virtual_host,
                      NiceMock<Envoy::Server::Configuration::MockServerFactoryContext>& context,
                      Envoy::Stats::Scope& scope, const Envoy::Router::ConfigImpl& config)
      : VirtualHostImpl(virtual_host, Envoy::Router::OptionalHttpFilters(), config, context, scope,
                        Envoy::ProtobufMessage::getNullValidationVisitor(),
                        absl::optional<Envoy::Upstream::ClusterManager::ClusterInfoMaps>()){};
  ~MockVirtualHostImpl() override = default;

  // Router::VirtualHost
  MOCK_METHOD(const std::string&, name, (), (const));
  MOCK_METHOD(const Envoy::Router::RouteSpecificFilterConfig*, perFilterConfig,
              (const std::string&), (const));
  MOCK_METHOD(bool, includeAttemptCountInRequest, (), (const));
  MOCK_METHOD(bool, includeAttemptCountInResponse, (), (const));
  MOCK_METHOD(Envoy::Upstream::RetryPrioritySharedPtr, retryPriority, ());
  MOCK_METHOD(Envoy::Upstream::RetryHostPredicateSharedPtr, retryHostPredicate, ());
  MOCK_METHOD(uint32_t, retryShadowBufferLimit, (), (const));

  mutable Envoy::Stats::TestUtil::TestSymbolTable symbol_table_;
  std::string name_{"fake_vhost"};
  mutable std::unique_ptr<Envoy::Stats::StatNameManagedStorage> stat_name_;
};

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SuperGlueFilter {

namespace v3 = envoy::extensions::filters::http::super_glue::v3;

class SuperGlueClientTest : public testing::Test {
public:
  SuperGlueClientTest() = default;
  void setup(const std::string& yaml, const std::string& route_yaml) {
    v3::SuperGlueClientGlobal global_config;
    v3::SuperGlueClientRoute route_config;
    ON_CALL(server_context_, clusterManager()).WillByDefault(ReturnRef(cluster_manager_));
    ON_CALL(server_context_, runtime()).WillByDefault(ReturnRef(runtime_));
    ON_CALL(server_context_, scope()).WillByDefault(ReturnRef(stats_));
    if (!route_yaml.empty()) {
      TestUtility::loadFromYamlAndValidate(route_yaml, route_config);

      route_config_ = std::make_shared<ClientFilterRouteConfig>(route_config, server_context_);
      ON_CALL(*decoder_callbacks_.route_, mostSpecificPerFilterConfig(FILTER_NAME))
          .WillByDefault(Return(route_config_.get()));
    }

    envoy::config::route::v3::VirtualHost virtual_host;
    if (!yaml.empty()) {
      Protobuf::Any config2;
      TestUtility::loadFromYaml(yaml, config2);
      virtual_host.mutable_typed_per_filter_config()->insert(
          {"envoy.filters.http.super-glue-client.1.0", config2});
    }
    virtual_host_ = std::make_shared<NiceMock<MockVirtualHostImpl>>(
        virtual_host, server_context_, stats_,
        Router::ConfigImpl(envoy::config::route::v3::RouteConfiguration(),
                           Router::OptionalHttpFilters(), server_context_,
                           ProtobufMessage::getNullValidationVisitor(), true));

    instance_ = std::make_shared<NiceMock<MockInstance>>();
    host_ = std::make_shared<NiceMock<Envoy::Upstream::MockHost>>();
    cluster_info_ = std::make_shared<NiceMock<Envoy::Upstream::MockClusterInfo>>();
    cluster_ = std::make_shared<NiceMock<Upstream::MockThreadLocalCluster>>();
    request_ = std::make_shared<NiceMock<Http::MockAsyncClientRequest>>(&async_client_);
    global_config_ = std::make_shared<ClientFilterGlobalConfig>(global_config, context_);
    filter_ = std::make_shared<ClientFilter>(global_config_, server_context_);

    EXPECT_CALL(decoder_callbacks_.route_->route_entry_, virtualHost())
        .WillRepeatedly(ReturnRef(*virtual_host_));
    ON_CALL(decoder_callbacks_, streamInfo()).WillByDefault(ReturnRef(stream_info_));
    ON_CALL(stream_info_, upstreamClusterInfo()).WillByDefault(Return(cluster_info_));

    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
  }

public:
  std::shared_ptr<ClientFilter> filter_;
  std::shared_ptr<Router::ConfigImpl> configImpl_;
  std::shared_ptr<ClientFilterGlobalConfig> global_config_;
  Envoy::Router::RouteSpecificFilterConfigConstSharedPtr route_config_;
  absl::optional<std::shared_ptr<NiceMock<Envoy::Upstream::MockClusterInfo>>> cluster_info_;

  NiceMock<Http::MockAsyncClient> async_client_;
  NiceMock<Envoy::Runtime::MockLoader> runtime_;
  NiceMock<Envoy::Runtime::MockSnapshot> snapshot_;
  NiceMock<Envoy::Stats::MockIsolatedStatsStore> stats_;
  NiceMock<Upstream::MockClusterManager> cluster_manager_;
  NiceMock<Server::Configuration::MockFactoryContext> context_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Server::Configuration::MockServerFactoryContext> server_context_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info_;
  std::shared_ptr<NiceMock<MockInstance>> instance_;
  std::shared_ptr<NiceMock<Envoy::Upstream::MockHost>> host_;
  std::shared_ptr<NiceMock<MockVirtualHostImpl>> virtual_host_;
  std::shared_ptr<NiceMock<Http::MockAsyncClientRequest>> request_;
  std::shared_ptr<NiceMock<Upstream::MockThreadLocalCluster>> cluster_;
};

// 验证没有VH配置时，插件放行
TEST_F(SuperGlueClientTest, NoConfigFilterPass) {
  const std::string yaml = R"(
    cluster_list:
      cluster: acl
      unhealthy_bypass: false
      timeout_bypass: false
      request_timeout: 1s
    rules:
    - rule_name: '1'
      enable: true
      matchers:
      - prefix: /login
    percent:
      numerator: 100
      denominator: HUNDRED
  )";
  setup("", yaml);

  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(filter_->decodeHeaders(headers, true), Http::FilterHeadersStatus::Continue);
}

// 验证没有安全资源池，插件放行
TEST_F(SuperGlueClientTest, NoThirdServerFilterPass) {
  const std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.super_glue.v3.SuperGlueClientRoute
    rules:
    - rule_name: '1'
      enable: true
      matchers:
      - prefix: /login
    percent:
      numerator: 100
      denominator: HUNDRED
  )";
  setup(yaml_local, "");

  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(filter_->decodeHeaders(headers, true), Http::FilterHeadersStatus::Continue);
}

// 验证匹配规则失败，不进行编排
TEST_F(SuperGlueClientTest, MatchRuleFaild) {
  const std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.super_glue.v3.SuperGlueClientRoute
    cluster_list:
      cluster: acl
      unhealthy_bypass: false
      timeout_bypass: false
      request_timeout: 1s
    rules:
    - rule_name: '1'
      enable: true
      matchers:
      - prefix: /login
    percent:
      numerator: 100
      denominator: HUNDRED
  )";
  setup(yaml_local, "");

  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("/fake");
  EXPECT_EQ(filter_->decodeHeaders(headers, true), Http::FilterHeadersStatus::Continue);
}

// 验证获取安全资源集群为空，插件放行
TEST_F(SuperGlueClientTest, ThirdServerNull) {
  const std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.super_glue.v3.SuperGlueClientRoute
    cluster_list:
      cluster: "acl"
      unhealthy_bypass: false
      timeout_bypass: false
      request_timeout: 1s
    rules:
    - rule_name: '1'
      enable: true
      matchers:
      - prefix: /
    percent:
      numerator: 100
      denominator: HUNDRED
  )";
  setup(yaml_local, "");

  envoy::type::v3::FractionalPercent percent;
  percent.set_denominator(envoy::type::v3::FractionalPercent::HUNDRED);
  percent.set_numerator(100);
  EXPECT_CALL(runtime_, snapshot()).WillOnce(ReturnRef(snapshot_));
  EXPECT_CALL(snapshot_, featureEnabled(_, FractionalPercentEq(percent))).WillOnce(Return(true));
  EXPECT_CALL(cluster_manager_, getThreadLocalCluster("acl")).WillOnce(Return(nullptr));

  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("/fake");
  EXPECT_EQ(filter_->decodeHeaders(headers, true), Http::FilterHeadersStatus::Continue);
}

// 验证集群不健康，bypass为false
TEST_F(SuperGlueClientTest, UnhealthyBypassFalse) {
  const std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.super_glue.v3.SuperGlueClientRoute
    cluster_list:
      cluster: acl
      unhealthy_bypass: false
      timeout_bypass: false
      request_timeout: 1s
    rules:
    - rule_name: '1'
      enable: true
      matchers:
      - prefix: /
    percent:
      numerator: 100
      denominator: HUNDRED
  )";
  setup(yaml_local, "");

  envoy::type::v3::FractionalPercent percent;
  percent.set_denominator(envoy::type::v3::FractionalPercent::HUNDRED);
  percent.set_numerator(100);
  EXPECT_CALL(runtime_, snapshot()).WillOnce(ReturnRef(snapshot_));
  EXPECT_CALL(snapshot_, featureEnabled(_, FractionalPercentEq(percent))).WillOnce(Return(true));
  EXPECT_CALL(cluster_manager_, getThreadLocalCluster(_)).WillOnce(Return(cluster_.get()));
  EXPECT_CALL(cluster_->lb_, peekAnotherHost(_)).WillOnce(Return(nullptr));

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::ServiceUnavailable, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)>,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::ServiceUnavailable, code);
        EXPECT_EQ("No healthy security resource, request rejection", body);
        EXPECT_EQ(absl::nullopt, grpc_status);
        EXPECT_EQ("", details);
      }));

  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("/fake");
  EXPECT_EQ(filter_->decodeHeaders(headers, true), Http::FilterHeadersStatus::Continue);
}

// 验证集群不健康，bypass为true
TEST_F(SuperGlueClientTest, UnhealthyBypassTrue) {
  const std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.super_glue.v3.SuperGlueClientRoute
    cluster_list:
      cluster: acl
      unhealthy_bypass: true
      timeout_bypass: false
      request_timeout: 1s
    rules:
    - rule_name: '1'
      enable: true
      matchers:
      - prefix: /
    percent:
      numerator: 100
      denominator: HUNDRED
  )";
  setup(yaml_local, "");

  envoy::type::v3::FractionalPercent percent;
  percent.set_denominator(envoy::type::v3::FractionalPercent::HUNDRED);
  percent.set_numerator(100);
  EXPECT_CALL(runtime_, snapshot()).WillOnce(ReturnRef(snapshot_));
  EXPECT_CALL(snapshot_, featureEnabled(_, FractionalPercentEq(percent))).WillOnce(Return(true));
  EXPECT_CALL(cluster_manager_, getThreadLocalCluster(_)).WillOnce(Return(cluster_.get()));
  EXPECT_CALL(cluster_->lb_, peekAnotherHost(_)).WillOnce(Return(nullptr));

  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("/fake");
  EXPECT_EQ(filter_->decodeHeaders(headers, true), Http::FilterHeadersStatus::Continue);
}

// 验证安全资源池超时，bypas为false
TEST_F(SuperGlueClientTest, OnSuccessTimeoutBypassFalse) {
  const std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.super_glue.v3.SuperGlueClientRoute
    cluster_list:
      cluster: acl
      unhealthy_bypass: true
      timeout_bypass: false
      request_timeout: 1s
    rules:
    - rule_name: '1'
      enable: true
      matchers:
      - prefix: /
    percent:
      numerator: 100
      denominator: HUNDRED
  )";
  setup(yaml_local, "");

  envoy::type::v3::FractionalPercent percent;
  percent.set_denominator(envoy::type::v3::FractionalPercent::HUNDRED);
  percent.set_numerator(100);
  EXPECT_CALL(runtime_, snapshot()).WillOnce(ReturnRef(snapshot_));
  EXPECT_CALL(snapshot_, featureEnabled(_, FractionalPercentEq(percent))).WillOnce(Return(true));

  const std::string addr = "fake_ip";
  EXPECT_CALL(cluster_manager_, getThreadLocalCluster(_)).WillOnce(Return(cluster_.get()));
  EXPECT_CALL(cluster_->lb_, peekAnotherHost(_)).WillOnce(Return(host_));
  EXPECT_CALL(*host_, address()).WillRepeatedly(Return(instance_));
  EXPECT_CALL(*instance_, asString()).WillRepeatedly(ReturnRef(addr));
  EXPECT_CALL(*cluster_, httpAsyncClient()).WillRepeatedly(ReturnRef(async_client_));

  EXPECT_CALL(async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        Http::ResponseMessagePtr response(new Http::ResponseMessageImpl(
            Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status",
            "504"}}}));
        callbacks.onSuccess(*request_, std::move(response));
        return request_.get();
      }));
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::GatewayTimeout, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)>,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::GatewayTimeout, code);
        EXPECT_EQ("Security resource pool request timeout", body);
        EXPECT_EQ(absl::nullopt, grpc_status);
        EXPECT_EQ("", details);
      }));

  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("/fake");
  EXPECT_EQ(filter_->decodeHeaders(headers, true), Http::FilterHeadersStatus::StopIteration);
}

// 验证安全资源池超时，bypas为true
TEST_F(SuperGlueClientTest, OnSuccessTimeoutBypassTrue) {
  const std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.super_glue.v3.SuperGlueClientRoute
    cluster_list:
      cluster: acl
      unhealthy_bypass: true
      timeout_bypass: true
      request_timeout: 1s
    rules:
    - rule_name: '1'
      enable: true
      matchers:
      - prefix: /
    percent:
      numerator: 100
      denominator: HUNDRED
  )";
  setup(yaml_local, "");

  envoy::type::v3::FractionalPercent percent;
  percent.set_denominator(envoy::type::v3::FractionalPercent::HUNDRED);
  percent.set_numerator(100);
  EXPECT_CALL(runtime_, snapshot()).WillOnce(ReturnRef(snapshot_));
  EXPECT_CALL(snapshot_, featureEnabled(_, FractionalPercentEq(percent))).WillOnce(Return(true));

  const std::string addr = "fake_ip";
  EXPECT_CALL(cluster_manager_, getThreadLocalCluster(_)).WillOnce(Return(cluster_.get()));
  EXPECT_CALL(cluster_->lb_, peekAnotherHost(_)).WillOnce(Return(host_));
  EXPECT_CALL(*host_, address()).WillRepeatedly(Return(instance_));
  EXPECT_CALL(*instance_, asString()).WillRepeatedly(ReturnRef(addr));
  EXPECT_CALL(*cluster_, httpAsyncClient()).WillRepeatedly(ReturnRef(async_client_));

  EXPECT_CALL(async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        Http::ResponseMessagePtr response(new Http::ResponseMessageImpl(
            Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status",
            "504"}}}));
        callbacks.onSuccess(*request_, std::move(response));
        return request_.get();
      }));
  EXPECT_CALL(decoder_callbacks_, continueDecoding());

  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("/fake");
  EXPECT_EQ(filter_->decodeHeaders(headers, true), Http::FilterHeadersStatus::StopIteration);
}

// 验证安全资源拒绝
TEST_F(SuperGlueClientTest, ThirdServerRefuse) {
  const std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.super_glue.v3.SuperGlueClientRoute
    cluster_list:
      cluster: acl
      unhealthy_bypass: true
      timeout_bypass: true
      request_timeout: 1s
    rules:
    - rule_name: '1'
      enable: true
      matchers:
      - prefix: /
    percent:
      numerator: 100
      denominator: HUNDRED
  )";
  setup(yaml_local, "");

  envoy::type::v3::FractionalPercent percent;
  percent.set_denominator(envoy::type::v3::FractionalPercent::HUNDRED);
  percent.set_numerator(100);
  EXPECT_CALL(runtime_, snapshot()).WillOnce(ReturnRef(snapshot_));
  EXPECT_CALL(snapshot_, featureEnabled(_, FractionalPercentEq(percent))).WillOnce(Return(true));

  const std::string addr = "fake_ip";
  EXPECT_CALL(cluster_manager_, getThreadLocalCluster(_)).WillOnce(Return(cluster_.get()));
  EXPECT_CALL(cluster_->lb_, peekAnotherHost(_)).WillOnce(Return(host_));
  EXPECT_CALL(*host_, address()).WillRepeatedly(Return(instance_));
  EXPECT_CALL(*instance_, asString()).WillRepeatedly(ReturnRef(addr));
  EXPECT_CALL(*cluster_, httpAsyncClient()).WillRepeatedly(ReturnRef(async_client_));

  EXPECT_CALL(async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        Http::ResponseMessagePtr response(new Http::ResponseMessageImpl(
            Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status",
            "200"}}}));
        callbacks.onSuccess(*request_, std::move(response));
        return request_.get();
      }));
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::Forbidden, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)>,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::Forbidden, code);
        EXPECT_EQ("", body);
        EXPECT_EQ(absl::nullopt, grpc_status);
        EXPECT_EQ("", details);
      }));

  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("/fake");
  EXPECT_EQ(filter_->decodeHeaders(headers, true), Http::FilterHeadersStatus::StopIteration);
}

// 验证安全资源放行
TEST_F(SuperGlueClientTest, ThirdServerPass) {
  const std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.super_glue.v3.SuperGlueClientRoute
    cluster_list:
      cluster: acl
      unhealthy_bypass: true
      timeout_bypass: true
      request_timeout: 1s
    rules:
    - rule_name: '1'
      enable: true
      matchers:
      - prefix: /
    percent:
      numerator: 100
      denominator: HUNDRED
  )";
  setup(yaml_local, "");

  envoy::type::v3::FractionalPercent percent;
  percent.set_denominator(envoy::type::v3::FractionalPercent::HUNDRED);
  percent.set_numerator(100);
  EXPECT_CALL(runtime_, snapshot()).WillOnce(ReturnRef(snapshot_));
  EXPECT_CALL(snapshot_, featureEnabled(_, FractionalPercentEq(percent))).WillOnce(Return(true));

  const std::string addr = "fake_ip";
  EXPECT_CALL(cluster_manager_, getThreadLocalCluster(_)).WillOnce(Return(cluster_.get()));
  EXPECT_CALL(cluster_->lb_, peekAnotherHost(_)).WillOnce(Return(host_));
  EXPECT_CALL(*host_, address()).WillRepeatedly(Return(instance_));
  EXPECT_CALL(*instance_, asString()).WillRepeatedly(ReturnRef(addr));
  EXPECT_CALL(*cluster_, httpAsyncClient()).WillRepeatedly(ReturnRef(async_client_));

  EXPECT_CALL(async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        Http::ResponseMessagePtr response(new Http::ResponseMessageImpl(
            Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status",
            "200"},{"X-SuperGlue-Flag", "hi"}}}));
        callbacks.onSuccess(*request_, std::move(response));
        return request_.get();
      }));
  EXPECT_CALL(decoder_callbacks_, continueDecoding());

  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("/fake");
  EXPECT_EQ(filter_->decodeHeaders(headers, true), Http::FilterHeadersStatus::StopIteration);
}

// 验证安全资源请求发送失败，bypass为false
TEST_F(SuperGlueClientTest, OnFailureTimeOutBypassFalse) {
  const std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.super_glue.v3.SuperGlueClientRoute
    cluster_list:
      cluster: acl
      unhealthy_bypass: true
      timeout_bypass: false
      request_timeout: 1s
    rules:
    - rule_name: '1'
      enable: true
      matchers:
      - prefix: /
    percent:
      numerator: 100
      denominator: HUNDRED
  )";
  setup(yaml_local, "");

  envoy::type::v3::FractionalPercent percent;
  percent.set_denominator(envoy::type::v3::FractionalPercent::HUNDRED);
  percent.set_numerator(100);
  EXPECT_CALL(runtime_, snapshot()).WillOnce(ReturnRef(snapshot_));
  EXPECT_CALL(snapshot_, featureEnabled(_, FractionalPercentEq(percent))).WillOnce(Return(true));

  const std::string addr = "fake_ip";
  EXPECT_CALL(cluster_manager_, getThreadLocalCluster(_)).WillOnce(Return(cluster_.get()));
  EXPECT_CALL(cluster_->lb_, peekAnotherHost(_)).WillOnce(Return(host_));
  EXPECT_CALL(*host_, address()).WillRepeatedly(Return(instance_));
  EXPECT_CALL(*instance_, asString()).WillRepeatedly(ReturnRef(addr));
  EXPECT_CALL(*cluster_, httpAsyncClient()).WillRepeatedly(ReturnRef(async_client_));

  EXPECT_CALL(async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        callbacks.onFailure(*request_, Http::AsyncClient::FailureReason::Reset);
        return request_.get();
      }));
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::RequestTimeout, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)>,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::RequestTimeout, code);
        EXPECT_EQ("Security resource pool request timeout", body);
        EXPECT_EQ(absl::nullopt, grpc_status);
        EXPECT_EQ("", details);
      }));

  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("/fake");
  EXPECT_EQ(filter_->decodeHeaders(headers, true), Http::FilterHeadersStatus::StopIteration);
}

// 验证安全资源请求发送失败，bypass为true
TEST_F(SuperGlueClientTest, OnFailureTimeOutBypassTrue) {
  const std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.super_glue.v3.SuperGlueClientRoute
    cluster_list:
      cluster: acl
      unhealthy_bypass: true
      timeout_bypass: true
      request_timeout: 1s
    rules:
    - rule_name: '1'
      enable: true
      matchers:
      - prefix: /
    percent:
      numerator: 100
      denominator: HUNDRED
  )";
  setup(yaml_local, "");

  envoy::type::v3::FractionalPercent percent;
  percent.set_denominator(envoy::type::v3::FractionalPercent::HUNDRED);
  percent.set_numerator(100);
  EXPECT_CALL(runtime_, snapshot()).WillOnce(ReturnRef(snapshot_));
  EXPECT_CALL(snapshot_, featureEnabled(_, FractionalPercentEq(percent))).WillOnce(Return(true));

  const std::string addr = "fake_ip";
  EXPECT_CALL(cluster_manager_, getThreadLocalCluster(_)).WillOnce(Return(cluster_.get()));
  EXPECT_CALL(cluster_->lb_, peekAnotherHost(_)).WillOnce(Return(host_));
  EXPECT_CALL(*host_, address()).WillRepeatedly(Return(instance_));
  EXPECT_CALL(*instance_, asString()).WillRepeatedly(ReturnRef(addr));
  EXPECT_CALL(*cluster_, httpAsyncClient()).WillRepeatedly(ReturnRef(async_client_));

  EXPECT_CALL(async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        callbacks.onFailure(*request_, Http::AsyncClient::FailureReason::Reset);
        return request_.get();
      }));
  EXPECT_CALL(decoder_callbacks_, continueDecoding());

  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("/fake");
  EXPECT_EQ(filter_->decodeHeaders(headers, true), Http::FilterHeadersStatus::StopIteration);
}

// 验证存在请求体
TEST_F(SuperGlueClientTest, HasRequestBody) {
  const std::string yaml_local = R"(
    "@type": type.googleapis.com/envoy.extensions.filters.http.super_glue.v3.SuperGlueClientRoute
    cluster_list:
      cluster: acl
      unhealthy_bypass: true
      timeout_bypass: true
      request_timeout: 1s
    rules:
    - rule_name: '1'
      enable: true
      matchers:
      - prefix: /
    percent:
      numerator: 100
      denominator: HUNDRED
  )";
  setup(yaml_local, "");

  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("/fake");
  EXPECT_EQ(filter_->decodeHeaders(headers, false), Http::FilterHeadersStatus::StopIteration);

  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillRepeatedly(Return(nullptr));

  envoy::type::v3::FractionalPercent percent;
  percent.set_denominator(envoy::type::v3::FractionalPercent::HUNDRED);
  percent.set_numerator(100);
  EXPECT_CALL(runtime_, snapshot()).WillOnce(ReturnRef(snapshot_));
  EXPECT_CALL(snapshot_, featureEnabled(_, FractionalPercentEq(percent))).WillOnce(Return(true));

  const std::string addr = "fake_ip";
  EXPECT_CALL(cluster_manager_, getThreadLocalCluster(_)).WillOnce(Return(cluster_.get()));
  EXPECT_CALL(cluster_->lb_, peekAnotherHost(_)).WillOnce(Return(host_));
  EXPECT_CALL(*host_, address()).WillRepeatedly(Return(instance_));
  EXPECT_CALL(*instance_, asString()).WillRepeatedly(ReturnRef(addr));
  EXPECT_CALL(*cluster_, httpAsyncClient()).WillRepeatedly(ReturnRef(async_client_));

  EXPECT_CALL(async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        Http::ResponseMessagePtr response(new Http::ResponseMessageImpl(Http::ResponseHeaderMapPtr{
            new Http::TestResponseHeaderMapImpl{{":status", "200"}, {"X-SuperGlue-Flag", "hi"}}}));
        callbacks.onSuccess(*request_, std::move(response));
        return request_.get();
      }));
  EXPECT_CALL(decoder_callbacks_, continueDecoding());

  std::string body = R"(
  {
    "code" : "df12",
    "src_ip" : "10.200.200.121"
  })";
  Buffer::OwnedImpl buffer(body);
  EXPECT_EQ(filter_->decodeData(buffer, true), Http::FilterDataStatus::StopIterationAndWatermark);
}

} // namespace SuperGlueFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy