#include <chrono>
#include <memory>
#include <iostream>

#include "test/mocks/http/mocks.h"
#include "test/mocks/network/connection.h"
#include "test/mocks/router/mocks.h"
#include "test/mocks/local_info/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/test_common/printers.h"
#include "test/test_common/test_runtime.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "source/common/http/header_map_impl.h"
#include "source/common/runtime/runtime_impl.h"
#include "source/common/router/config_impl.h"
#include "source/common/network/address_impl.h"
#include "source/common/json/json_loader.h"
#include "envoy/registry/registry.h"
#include "envoy/event/dispatcher.h"

#include "filters/api/envoy/extensions/filters/http/acl/v3/acl.pb.h"
#include "filters/source/extensions/filters/http/acl/acl_filter.h"

using testing::InSequence;
using testing::NiceMock;
using testing::Return;
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
namespace AclFilter {

namespace v3 = envoy::extensions::filters::http::acl::v3;

class RuleTest : public testing::Test {
public:
  void SetUp() override {
    EXPECT_CALL(callbacks_.route_->route_entry_, clusterName())
        .WillRepeatedly(testing::ReturnRef(upstream_));
  }
  void setAddressToReturn(const std::string& address) {
    callbacks_.stream_info_.downstream_connection_info_provider_->setRemoteAddress(
        Network::Utility::resolveUrl(address));
  }
  void setUpstream(const std::string& upstream) { upstream_ = upstream; }

  v3::Rule proto_rule;
  Http::TestRequestHeaderMapImpl sample_request{
      {":authority", "localhost:8888"},
      {":path", "/anything?aa=bb&cc=dd"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "curl/7.68.0"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
      {"x-envoy-expected-rq-timeout-ms", "15000"},
      {"foo", "bar"},
      {"end-user", "envoy"},
      {"ouryun", "123"},
  };
  Envoy::Network::Address::InstanceConstSharedPtr source_ip_;
  std::string user_name_;
  std::string upstream_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks_;
};

TEST_F(RuleTest, EmptyRule) {
  const std::string yaml = "{}";

  TestUtility::loadFromJson(yaml, proto_rule);

  Rule rt_rule(proto_rule);
  EXPECT_TRUE(rt_rule.match(callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
      sample_request, user_name_));
  EXPECT_EQ(rt_rule.action(), v3::Action::ALLOW);
}

TEST_F(RuleTest, IPRangeMatch) {
  setAddressToReturn("tcp://1.2.3.4:80");

  std::string yaml = R"EOF(
        enable: true
        match:
          ip_list:
            ip_range:
              start_ip: 1.2.3.0
              end_ip: 1.2.3.100
            invert: false
        act: ALLOW
  )EOF";

  TestUtility::loadFromYaml(yaml, proto_rule);
  Rule rt_rule(proto_rule);
  bool matched = rt_rule.match(callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
      sample_request, user_name_);
  EXPECT_TRUE(matched);
}

TEST_F(RuleTest, IPRangeMatchInvert) {
  setAddressToReturn("tcp://1.2.3.4:80");

  std::string yaml = R"EOF(
        enable: true
        match:
          ip_list:
            ip_range:
              start_ip: 1.2.3.0
              end_ip: 1.2.3.100
            invert: true
        act: ALLOW
  )EOF";

  TestUtility::loadFromYaml(yaml, proto_rule);
  Rule rt_rule(proto_rule);
  bool matched = rt_rule.match(callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
      sample_request, user_name_);
  EXPECT_FALSE(matched);
}


TEST_F(RuleTest, IPListNOMatch) {
  setAddressToReturn("tcp://1.2.3.4:80");
  const std::string yaml = R"EOF(
        enable: true
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 1.2.3.5
                prefix_len: 32
            invert: false
        act: ALLOW
  )EOF";

  TestUtility::loadFromYaml(yaml, proto_rule);
  Rule rt_rule(proto_rule);
  bool matched = rt_rule.match(callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
      sample_request, user_name_);
  EXPECT_FALSE(matched);
}


TEST_F(RuleTest, IPListMatch) {
  setAddressToReturn("tcp://1.2.3.4:80");
  const std::string yaml = R"EOF(
        enable: true
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 1.2.3.4
                prefix_len: 32
            invert: false
        act: ALLOW
  )EOF";

  TestUtility::loadFromYaml(yaml, proto_rule);
  Rule rt_rule(proto_rule);
  bool matched = rt_rule.match(callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
      sample_request, user_name_);
  EXPECT_TRUE(matched);
}


TEST_F(RuleTest, UpstreamMatch) {
  setUpstream("ouryun");

  std::string yaml = R"EOF(
    upstream: ouryun
  )EOF";

  TestUtility::loadFromYaml(yaml, proto_rule);
  Rule rt_rule(proto_rule);
  bool matched = rt_rule.match(callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
      sample_request, user_name_);
  EXPECT_TRUE(matched);
}

TEST_F(RuleTest, TestEnable) {

  setAddressToReturn("tcp://1.2.3.4:80");

  std::string yaml = R"EOF(
        enable: true
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 1.2.3.4
                prefix_len: 32
            invert: false
        act: ALLOW
  )EOF";

  TestUtility::loadFromYaml(yaml, proto_rule);
  Rule rt_rule(proto_rule);
  bool matched = rt_rule.match(callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
      sample_request, user_name_);
  EXPECT_TRUE(matched);
}

TEST_F(RuleTest, TestPath) {

  setAddressToReturn("tcp://1.2.3.4:80");

  std::string yaml = R"EOF(
        enable: true
        match:
          prefix: /
          path_invert: false
        act: ALLOW
  )EOF";

  TestUtility::loadFromYaml(yaml, proto_rule);
  Rule rt_rule(proto_rule);
  bool matched = rt_rule.match(callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
      sample_request, user_name_);
  EXPECT_TRUE(matched);
}

TEST_F(RuleTest, TestPathInvert) {

  setAddressToReturn("tcp://1.2.3.4:80");

  std::string yaml = R"EOF(
        enable: true
        match:
          prefix: /
          path_invert: true
        act: ALLOW
  )EOF";

  TestUtility::loadFromYaml(yaml, proto_rule);
  Rule rt_rule(proto_rule);
  bool matched = rt_rule.match(callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
      sample_request, user_name_);
  EXPECT_FALSE(matched);
}

TEST_F(RuleTest, TestHeader) {

  setAddressToReturn("tcp://1.2.3.4:80");

  std::string yaml = R"EOF(
        enable: true
        match:
          headers:
            name: "ouryun"
            contains_match: "123"
        act: ALLOW
  )EOF";

  TestUtility::loadFromYaml(yaml, proto_rule);
  Rule rt_rule(proto_rule);
  bool matched = rt_rule.match(callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
      sample_request, user_name_);
  EXPECT_TRUE(matched);
}

TEST_F(RuleTest, TestMethod) {

  setAddressToReturn("tcp://1.2.3.4:80");

  std::string yaml = R"EOF(
        enable: true
        match:
          methods: GET
          method_invert: false
        act: ALLOW
  )EOF";

  TestUtility::loadFromYaml(yaml, proto_rule);
  Rule rt_rule(proto_rule);
  bool matched = rt_rule.match(callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
      sample_request, user_name_);
  EXPECT_TRUE(matched);
}

TEST_F(RuleTest, TestMethodInvert) {

  setAddressToReturn("tcp://1.2.3.4:80");

  std::string yaml = R"EOF(
        enable: true
        match:
          methods: GET
          method_invert: true
        act: ALLOW
  )EOF";

  TestUtility::loadFromYaml(yaml, proto_rule);
  Rule rt_rule(proto_rule);
  bool matched = rt_rule.match(callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
      sample_request, user_name_);
  EXPECT_FALSE(matched);
}


TEST_F(RuleTest, TestQueryParameters) {

  setAddressToReturn("tcp://1.2.3.4:80");

  std::string yaml = R"EOF(
        enable: true
        match:
          query_parameters:
            name: "aa"
            value:
              contains: "123"
        act: ALLOW
  )EOF";

  TestUtility::loadFromYaml(yaml, proto_rule);
  Rule rt_rule(proto_rule);
  bool matched = rt_rule.match(callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
      sample_request, user_name_);
  EXPECT_FALSE(matched);
}

TEST_F(RuleTest, TestUser) {

  setAddressToReturn("tcp://1.2.3.4:80");

  std::string yaml = R"EOF(
        enable: true
        match:
          users:
            user_name: "ouryun,srhino"
        act: ALLOW
  )EOF";

  TestUtility::loadFromYaml(yaml, proto_rule);
  Rule rt_rule(proto_rule);
  bool matched = rt_rule.match(callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
      sample_request, user_name_);
  EXPECT_TRUE(matched);
}

// ip范围和ipset都存在满足其一即可
TEST_F(RuleTest, TestIp) {

  setAddressToReturn("tcp://1.2.3.4:80");

  std::string yaml = R"EOF(
        enable: true
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 1.2.3.4
                prefix_len: 32
            ip_range:
              start_ip: 1.2.3.0
              end_ip: 1.2.3.3
            invert: false
        act: ALLOW
        type: json
  )EOF";

  TestUtility::loadFromYaml(yaml, proto_rule);
  Rule rt_rule(proto_rule);
  bool matched = rt_rule.match(callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
      sample_request, user_name_);
  EXPECT_TRUE(matched);
}

class AclFilterTest : public testing::Test, public AclFilter {
public:
  AclFilterGlobalConfigSharedPtr setupConfig() {
    envoy::extensions::filters::http::acl::v3::Acl proto_config;
    return std::make_shared<AclFilterGlobalConfig>(proto_config);
  }

  AclFilterTest() : AclFilter(setupConfig(), context_) {
    this->setDecoderFilterCallbacks(callbacks_);
    // this->AclFilter::setDecoderFilterCallbacks(callbacks_);
    // filter_.setDecoderFilterCallbacks(callbacks_);
  }
  void setupVH(std::string& yaml) {
    envoy::config::route::v3::VirtualHost virtual_host_proto;
    TestUtility::loadFromYaml(yaml, virtual_host_proto);
    virtual_host_ = std::make_unique<Router::VirtualHostImpl>(
        virtual_host_proto, Router::OptionalHttpFilters(),
        Router::ConfigImpl(envoy::config::route::v3::RouteConfiguration(),
                           Router::OptionalHttpFilters(), context_,
                           ProtobufMessage::getNullValidationVisitor(), true),
        context_, stats_, ProtobufMessage::getNullValidationVisitor(),
        absl::optional<Upstream::ClusterManager::ClusterInfoMaps>());

    EXPECT_CALL(callbacks_.route_->route_entry_, virtualHost())
        .WillRepeatedly(ReturnRef(*virtual_host_));
  }

  void SetUp() override {
    // filter_ = AclFilter(setupConfig());

    EXPECT_CALL(callbacks_.route_->route_entry_, clusterName())
        .WillRepeatedly(testing::ReturnRef(upstream_));

    // EXPECT_CALL(*virtual_host_, perFilterConfig(_)).WillRepeatedly(Return(reinterpret_cast<const
    // Envoy::Router::RouteSpecificFilterConfig*>((vh_config_.get())))); EXPECT_CALL(*virtual_host_,
    // perFilterConfig(_)).WillRepeatedly(Return(reinterpret_cast<const
    // Envoy::Router::RouteSpecificFilterConfig*>(0x123))); ON_CALL(*virtual_host_,
    // perFilterConfig(_)).WillByDefault(Return(reinterpret_cast<const
    // Envoy::Router::RouteSpecificFilterConfig*>(0x123))); dynamic_cast<const
    // Envoy::Router::VirtualHostImpl*>(&(callbacks_.route()->routeEntry()->virtualHost()));
    // EXPECT_CALL(callbacks_.route_->route_entry_.virtualHost(),
    // perFilterConfig(_)).WillRepeatedly(Return(reinterpret_cast<const
    // Envoy::Router::RouteSpecificFilterConfig*>((vh_config_.get()))));

    // filter_.setDecoderFilterCallbacks(callbacks_);
    // this->AclFilter::setDecoderFilterCallbacks(callbacks_);
  }

  void setAddressToReturn(const std::string& address) {
    callbacks_.stream_info_.downstream_connection_info_provider_->setRemoteAddress(
        Network::Utility::resolveUrl(address));
  }

  void setUpstream(const std::string& upstream) { upstream_ = upstream; }

  const Envoy::Router::VirtualHostImpl* getVirtualHostImpl() {
    Envoy::Router::RouteConstSharedPtr route = callbacks_.route();
    if (route == nullptr) {
      return nullptr;
    }
    const Envoy::Router::RouteEntry* route_entry = route->routeEntry();
    if (route_entry == nullptr) {
      return nullptr;
    }
    const Envoy::Router::VirtualHostImpl* virtual_host_impl =
        dynamic_cast<const Envoy::Router::VirtualHostImpl*>(&(route_entry->virtualHost()));
    return virtual_host_impl;
  }

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks_;
  NiceMock<Server::Configuration::MockServerFactoryContext> context_;
  NiceMock<Stats::MockIsolatedStatsStore> stats_;
  std::unique_ptr<Router::VirtualHostImpl> virtual_host_;
  NiceMock<Network::MockConnection> connection_{};

  Envoy::Network::Address::InstanceConstSharedPtr source_ip_;
  std::string upstream_;

  Http::TestRequestHeaderMapImpl sample_request{
      {":authority", "localhost:8888"},
      {":path", "/anything?aa=bb&cc=dd"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "curl/7.68.0"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
      {"x-envoy-expected-rq-timeout-ms", "15000"},
      {"foo", "bar"},
      {"end-user", "envoy"},
  };
};


TEST_F(AclFilterTest, VHNoKey) {
  std::string yaml = R"EOF(
    name: fakevh
    domains: ["*"]
  )EOF";
  setupVH(yaml);
  // vh_config_ = nullptr;
  auto status = decodeHeaders(sample_request, false);
  EXPECT_EQ(status, Http::FilterHeadersStatus::Continue);
}

TEST_F(AclFilterTest, VHDefaultConfig) {
  std::string yaml = R"EOF(
    name: fakevh
    domains: ["*"]
    typed_per_filter_config:
      envoy.filters.http.acl.1.0:
        "@type": type.googleapis.com/envoy.extensions.filters.http.acl.v3.AclPerRoute
  )EOF";
  setupVH(yaml);

  auto status = decodeHeaders(sample_request, false);
  EXPECT_EQ(status, Http::FilterHeadersStatus::Continue);
}



TEST_F(AclFilterTest, VHDeny) {
  std::string yaml = R"EOF(
    name: fakevh
    domains: ["*"]
    typed_per_filter_config:
      envoy.filters.http.acl.1.0:
        "@type": type.googleapis.com/envoy.extensions.filters.http.acl.v3.AclPerRoute
        rules:
          match:
            ip_list:
              ip_set:
                list:
                  address_prefix: 127.0.0.1
                  prefix_len: 32
          id: 3
          enable: true
          act: DENY
          deny_context: "virtual host deny context"
          match:
            ip_list:
              ip_set:
                list:
                  address_prefix: 127.0.0.1
                  prefix_len: 32
          id: 1
          enable: true
          type: html
          act: DENY
          deny_context: "virtual host deny context"
  )EOF";
  setupVH(yaml);

  // 确认一下MOCK VH数据是否成功
  ASSERT_EQ(&(callbacks_.route()->routeEntry()->virtualHost()),
            reinterpret_cast<Envoy::Router::VirtualHost*>(virtual_host_.get()));

  const Envoy::Router::VirtualHostImpl* virtual_host_impl = getVirtualHostImpl();
  // 确认mock的perFilterConfig函数和真实调用的函数相同(因为不是虚函数,gMock继承无法实现mock效果)
  ASSERT_EQ(virtual_host_->perFilterConfig("envoy.filters.http.acl.1.0"),
            virtual_host_impl->perFilterConfig("envoy.filters.http.acl.1.0"));

  auto status = decodeHeaders(sample_request, true);
  EXPECT_EQ(status, Http::FilterHeadersStatus::StopIteration);
}

TEST_F(AclFilterTest, VHALLOW) {
  std::string yaml = R"EOF(
    name: fakevh
    domains: ["*"]
    typed_per_filter_config:
      envoy.filters.http.acl.1.0:
        "@type": type.googleapis.com/envoy.extensions.filters.http.acl.v3.AclPerRoute
        rules:
          match:
          id: 2
          enable: true
          act: ALLOW
          deny_context: "virtual host allow context"
          id: 1
          enable: true
          act: ALLOW
          deny_context: "virtual host deny context"
  )EOF";
  setupVH(yaml);

  // 确认一下MOCK VH数据是否成功
  ASSERT_EQ(&(callbacks_.route()->routeEntry()->virtualHost()),
            reinterpret_cast<Envoy::Router::VirtualHost*>(virtual_host_.get()));

  const Envoy::Router::VirtualHostImpl* virtual_host_impl = getVirtualHostImpl();
  // 确认mock的perFilterConfig函数和真实调用的函数相同(因为不是虚函数,gMock继承无法实现mock效果)
  ASSERT_EQ(virtual_host_->perFilterConfig("envoy.filters.http.acl.1.0"),
            virtual_host_impl->perFilterConfig("envoy.filters.http.acl.1.0"));

  auto status = decodeHeaders(sample_request, true);
  EXPECT_EQ(status, Http::FilterHeadersStatus::Continue);
}

TEST_F(AclFilterTest, IpListVHALLOW) {
  std::string yaml = R"EOF(
    name: fakevh
    domains: ["*"]
    typed_per_filter_config:
      envoy.filters.http.acl.1.0:
        "@type": type.googleapis.com/envoy.extensions.filters.http.acl.v3.AclPerRoute
        rules:
          match:
            ip_list:
              ip_set:
                list:
                  address_prefix: 127.0.0.1
                  prefix_len: 32
          id: 2
          enable: true
          act: ALLOW
          deny_context: "virtual host allow context"
  )EOF";
  setupVH(yaml);

  // 确认一下MOCK VH数据是否成功
  ASSERT_EQ(&(callbacks_.route()->routeEntry()->virtualHost()),
            reinterpret_cast<Envoy::Router::VirtualHost*>(virtual_host_.get()));

  const Envoy::Router::VirtualHostImpl* virtual_host_impl = getVirtualHostImpl();
  // 确认mock的perFilterConfig函数和真实调用的函数相同(因为不是虚函数,gMock继承无法实现mock效果)
  ASSERT_EQ(virtual_host_->perFilterConfig("envoy.filters.http.acl.1.0"),
            virtual_host_impl->perFilterConfig("envoy.filters.http.acl.1.0"));

  auto status = decodeHeaders(sample_request, true);
  EXPECT_EQ(status, Http::FilterHeadersStatus::Continue);
}

TEST_F(AclFilterTest, RouteDeny) {
  std::string yaml = R"EOF(
    name: fakevh
    domains: ["*"]
    typed_per_filter_config:
      envoy.filters.http.acl.1.0:
        "@type": type.googleapis.com/envoy.extensions.filters.http.acl.v3.AclPerRoute
        rules:
          match:
          id: 2
          enable: true
          act: ALLOW
          deny_context: "virtual host allow context"
  )EOF";
  setupVH(yaml);

   yaml =  R"EOF(
        rules:
          match:
            ip_list:
              ip_set:
                list:
                  address_prefix: 127.0.0.1
                  prefix_len: 32
          id: 1
          enable: true
          act: ALLOW
          deny_context: "route deny context"
  )EOF";
  v3::AclPerRoute acl_setting;
  TestUtility::loadFromYaml(yaml, acl_setting);
  setAddressToReturn("tcp://127.0.0.1:80");

  AclFilterRouteConfig route_settings(acl_setting);
  EXPECT_CALL(*callbacks_.route_, mostSpecificPerFilterConfig("envoy.filters.http.acl.1.0"))
      .WillRepeatedly(Return(&route_settings));

  const auto* vh_scoped_setting = dynamic_cast<const AclFilterRouteConfig*>(
      virtual_host_->perFilterConfig("envoy.filters.http.acl.1.0"));
  const auto* route_scoped_setting = dynamic_cast<const AclFilterRouteConfig*>(
      Http::Utility::resolveMostSpecificPerFilterConfig<AclFilterRouteConfig>(
          "envoy.filters.http.acl.1.0", callbacks_.route()));
  EXPECT_NE(vh_scoped_setting, nullptr);
  EXPECT_NE(route_scoped_setting, nullptr);
  auto status = decodeHeaders(sample_request, true);
  EXPECT_EQ(status, Http::FilterHeadersStatus::Continue);
}

struct NodeInfo {
  std::string nodeid;
  std::string gwid;
  std::string vsname;
  std::string cluster;
  std::string filtername;
  std::string filterinstance;
};
static const NodeInfo g_node_Info{
    "nodeid-1", "route-1", "vs-1", "cluster-1", "envoy.filters.http.acl.1.0", "instance123213"};

static const std::string g_cluster_name_direct_ = "_direct_response";
static const std::string g_cluster_name_redirect_ = "_redirect";

class AclFilterLogTest : public testing::Test {
public:
  AclFilterLogTest() = default;
  AclFilterGlobalConfigSharedPtr setupConfig() {
    envoy::extensions::filters::http::acl::v3::Acl proto_config;
    return std::make_shared<AclFilterGlobalConfig>(proto_config);
  }
  // for key test
  void setupForKey(std::function<void()> setupRoute, const std::string& yaml) {

    cluster_info_ptr_ = std::make_shared<NiceMock<Envoy::Upstream::MockClusterInfo>>();
    cluster_info_ = cluster_info_ptr_;

    (*context_.bootstrap_.mutable_node()->mutable_id()) = g_node_Info.nodeid;

    { // set filter's name into metadata
      ProtobufWkt::Struct metadata;
      auto& fields = *metadata.mutable_fields();
      fields["filter_name"].set_string_value(g_node_Info.filtername + "." +
                                             g_node_Info.filterinstance); // 单元测试在这里传递参数
      (*stream_info_.metadata_.mutable_filter_metadata())[HttpFilterNames::get().Composite]
          .MergeFrom(metadata);
    }
    { // vh
      envoy::config::route::v3::RouteConfiguration routeConfiguration;
      *routeConfiguration.mutable_name() = g_node_Info.gwid;

      configImpl_ = std::make_shared<Router::ConfigImpl>(
          routeConfiguration, Router::OptionalHttpFilters(), context_,
          ProtobufMessage::getNullValidationVisitor(), true);

      envoy::config::route::v3::VirtualHost virtual_host;
      *virtual_host.mutable_name() = g_node_Info.vsname;
      Protobuf::Any config2;
      TestUtility::loadFromYaml(yaml, config2);
      virtual_host.mutable_typed_per_filter_config()->insert({g_node_Info.filtername, config2});
      virtual_host_ = std::make_unique<NiceMock<Router::MockVirtualHostImpl>>(
          virtual_host, context_, stats_, *configImpl_);
    }
    setupRoute();
    downstream_connection_info_provider_ =
        std::make_shared<Network::ConnectionInfoSetterImpl>(nullptr, nullptr);
    // ON_CALL(encoder_callbacks_, streamInfo()).WillByDefault(ReturnRef(stream_info_));
    ON_CALL(decoder_callbacks_, streamInfo()).WillByDefault(ReturnRef(stream_info_));
    ON_CALL(stream_info_, upstreamClusterInfo()).WillByDefault(Return(cluster_info_));
    ON_CALL(stream_info_, downstreamAddressProvider())
        .WillByDefault(ReturnRef(*downstream_connection_info_provider_));

    filter_ = std::make_unique<AclFilter>(setupConfig(), context_);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    filter_->onStreamComplete();
  }
  void setAddressToReturn(const std::string& address) {
    // decoder_callbacks_.stream_info_.downstream_connection_info_provider_->setRemoteAddress(
    //     Network::Utility::resolveUrl(address));
    downstream_connection_info_provider_->setRemoteAddress(Network::Utility::resolveUrl(address));
  }
  NiceMock<Server::Configuration::MockServerFactoryContext> context_;
  std::shared_ptr<Router::ConfigImpl> configImpl_;
  std::shared_ptr<NiceMock<Envoy::Upstream::MockClusterInfo>> cluster_info_ptr_;
  absl::optional<Envoy::Upstream::ClusterInfoConstSharedPtr> cluster_info_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info_;
  NiceMock<Stats::MockIsolatedStatsStore> stats_;
  testing::NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;

  std::unique_ptr<NiceMock<Router::MockVirtualHostImpl>> virtual_host_;
  // std::unique_ptr<Router::VirtualHostImpl> virtual_host_;

  std::shared_ptr<NiceMock<Router::MyMockRoute>> myRoute_;
  std::shared_ptr<NiceMock<Router::MockRoute>> route_;
  std::unique_ptr<AclFilter> filter_;

  // Network::ConnectionInfoProviderSharedPtr downstream_connection_info_provider_;
  // Http::OverridableRemoteConnectionInfoSetterStreamInfo downstream_connection_info_provider_;
  std::shared_ptr<Network::ConnectionInfoSetterImpl>
      downstream_connection_info_provider_; // connection_info_provider_;
  // Address::InstanceConstSharedPtr remote_address_;

  Http::TestRequestHeaderMapImpl sample_request{
      {":authority", "localhost:8888"},
      {":path", "/anything?aa=bb&cc=dd"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "curl/7.68.0"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
      {"x-envoy-expected-rq-timeout-ms", "15000"},
      {"foo", "bar"},
      {"end-user", "envoy"},
  };
};

TEST_F(AclFilterLogTest, LogFormat) {
  auto setupRoute = [this]() {
    route_ = std::make_shared<NiceMock<Router::MockRoute>>();
    route_->route_entry_.cluster_name_ = g_node_Info.cluster;

    ON_CALL(route_->route_entry_, virtualHost()).WillByDefault(ReturnRef(*virtual_host_));
    ON_CALL(stream_info_, route()).WillByDefault(Return(route_));
    ON_CALL(decoder_callbacks_, route()).WillByDefault(Return(route_));
  };
  //deny  拒绝
  {
    std::string yaml = R"EOF(
      "@type": type.googleapis.com/envoy.extensions.filters.http.acl.v3.AclPerRoute
      rules:
        id: 1
        enable: true
        act: DENY
        deny_context: "virtual host deny context"
    )EOF";
    setupForKey(setupRoute, yaml);
    setAddressToReturn("tcp://1.2.3.4:5678");
    auto status = filter_->decodeHeaders(sample_request, false);
    EXPECT_EQ(status, Http::FilterHeadersStatus::StopIteration);

    auto log_json = MessageUtil::getJsonStringFromMessageOrDie(filter_->getLog(), false, true);
    Envoy::Json::ObjectSharedPtr log_json_object = Envoy::Json::Factory::loadFromString(log_json);
    EXPECT_EQ(log_json_object->getInteger("act"), v3::AclLog::DENY);
  }
  // record 仅记录
  {
    std::string yaml = R"EOF(
      "@type": type.googleapis.com/envoy.extensions.filters.http.acl.v3.AclPerRoute
      rules:
        id: 1
        enable: true
        act: RECORD
        deny_context: "virtual host record context"
    )EOF";
    setupForKey(setupRoute, yaml);
    setAddressToReturn("tcp://1.2.3.4:5678");
    auto status = filter_->decodeHeaders(sample_request, false);
    EXPECT_EQ(status, Http::FilterHeadersStatus::Continue);

    auto log_json = MessageUtil::getJsonStringFromMessageOrDie(filter_->getLog(), false, true);
    Envoy::Json::ObjectSharedPtr log_json_object = Envoy::Json::Factory::loadFromString(log_json);
    EXPECT_EQ(log_json_object->getInteger("act"), v3::AclLog::RECORD);
  }
  // allow  放行
  {
    {
    std::string yaml = R"EOF(
      "@type": type.googleapis.com/envoy.extensions.filters.http.acl.v3.AclPerRoute
      rules:
        id: 1
        enable: true
        act: ALLOW
        type: json
        deny_context: "virtual host allow context"
    )EOF";
      setupForKey(setupRoute, yaml);
      auto key = filter_->getFilterAccessLogKey();
      EXPECT_EQ(fmt::format("[{}].[{}].[{}].[{}]:[{}.{}]", g_node_Info.nodeid, g_node_Info.gwid,
                            g_node_Info.vsname, g_node_Info.cluster, g_node_Info.filtername,
                            g_node_Info.filterinstance),
                key);

      setAddressToReturn("tcp://1.2.3.4:5678");

      auto status = filter_->decodeHeaders(sample_request, false);
      EXPECT_EQ(status, Http::FilterHeadersStatus::Continue);

      auto log_json = MessageUtil::getJsonStringFromMessageOrDie(filter_->getLog(), false, true);
      Envoy::Json::ObjectSharedPtr log_json_object = Envoy::Json::Factory::loadFromString(log_json);
      EXPECT_EQ(log_json_object->getInteger("act"), v3::AclLog::ALLOW);
    }
    {
    std::string yaml = R"EOF(
      "@type": type.googleapis.com/envoy.extensions.filters.http.acl.v3.AclPerRoute
      rules:
        id: 1
        enable: true
        act: ALLOW
        type: html
        deny_context: "virtual host allow context"
    )EOF";
      setupForKey(setupRoute, yaml);
      auto key = filter_->getFilterAccessLogKey();
      EXPECT_EQ(fmt::format("[{}].[{}].[{}].[{}]:[{}.{}]", g_node_Info.nodeid, g_node_Info.gwid,
                            g_node_Info.vsname, g_node_Info.cluster, g_node_Info.filtername,
                            g_node_Info.filterinstance),
                key);

      setAddressToReturn("tcp://1.2.3.4:5678");

      auto status = filter_->decodeHeaders(sample_request, false);
      EXPECT_EQ(status, Http::FilterHeadersStatus::Continue);

      auto log_json = MessageUtil::getJsonStringFromMessageOrDie(filter_->getLog(), false, true);
      Envoy::Json::ObjectSharedPtr log_json_object = Envoy::Json::Factory::loadFromString(log_json);
      EXPECT_EQ(log_json_object->getInteger("act"), v3::AclLog::ALLOW);
    }
  }
}

} // namespace AclFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
