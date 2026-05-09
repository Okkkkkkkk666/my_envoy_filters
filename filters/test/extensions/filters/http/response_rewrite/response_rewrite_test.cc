#include "test/mocks/server/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

// // #include "source/extensions/compression/gzip/compressor/config.h"
// #include "envoy/event/timer.h"
// #include "source/extensions/compression/gzip/compressor/config.h"
// #include "test/integration/http_integration.h"
// #include "test/mocks/server/factory_context.h"
// #include "test/test_common/simulated_time_system.h"

// #include "envoy/test/integration/base_integration_test.h"

#include "test/mocks/compression/decompressor/mocks.h"
#include "test/mocks/compression/compressor/mocks.h"
// #include "source/extensions/compression/gzip/compressor/config.h"
// #include "source/extensions/compression/brotli/compressor/config.h"
// #include "source/extensions/compression/gzip/decompressor/config.h"
// #include "source/extensions/compression/brotli/decompressor/config.h"

#include "test/mocks/http/mocks.h"
#include "test/mocks/network/connection.h"
#include "test/mocks/router/mocks.h"
#include "test/mocks/local_info/mocks.h"

#include "test/test_common/printers.h"
#include "test/test_common/test_runtime.h"
#include "source/common/http/header_map_impl.h"
#include "source/common/runtime/runtime_impl.h"
#include "source/common/router/config_impl.h"
#include "source/common/json/json_loader.h"
#include "envoy/registry/registry.h"
#include "envoy/event/dispatcher.h"

#include "filters/source/extensions/filters/http/response_rewrite/config.h"
#include "filters/source/extensions/filters/http/response_rewrite/response_rewrite.h"
#include "filters/api/envoy/extensions/filters/http/response_rewrite/v3/response_rewrite.pb.h"
#include "filters/api/envoy/extensions/filters/http/response_rewrite/v3/response_rewrite_log.pb.h"

using Envoy::Http::LowerCaseString;
using testing::ByMove;
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
namespace ResponseRewrite {
namespace v3 = envoy::extensions::filters::http::response_rewrite::v3;
static const Http::TestResponseHeaderMapImpl response_headers([]() {
  Http::TestResponseHeaderMapImpl tmp_headers{
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
      {"bar", "foo"},
      {":foo", "bar"},
      {":bar", "foo"},
      {"end-user", "envoy"},
      {"content-length", "441"},
      {"content-type", "text/json; charset=utf-8"},
      {"Content-Encoding", "gzip"},
  };
  tmp_headers.setStatus(200);
  return tmp_headers;
}());
static const Http::TestRequestHeaderMapImpl request_headers([]() {
  Http::TestRequestHeaderMapImpl tmp_headers{
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
  return tmp_headers;
}());

class ResponseRewriteTest : public testing::Test {
public:
  ResponseRewriteGlobalConfigSharedPtr setupConfig(const std::string& yaml) {
    v3::ResponseRewriteGlobal proto_config;
    TestUtility::loadFromYamlAndValidate(yaml, proto_config);
    auto decompressor_gzip_factory =
        std::make_unique<NiceMock<Compression::Decompressor::MockDecompressorFactory>>();
    auto decompressor_brotli_factory =
        std::make_unique<NiceMock<Compression::Decompressor::MockDecompressorFactory>>();
    auto compressor_gzip_factory =
        std::make_unique<NiceMock<Compression::Compressor::MockCompressorFactory>>();
    auto compressor_brotli_factory =
        std::make_unique<NiceMock<Compression::Compressor::MockCompressorFactory>>();
    decompressor_factory_gzip_ = decompressor_gzip_factory.get();
    decompressor_factory_brotli_ = decompressor_brotli_factory.get();
    compressor_factory_gzip_ = compressor_gzip_factory.get();
    compressor_factory_brotli_ = compressor_brotli_factory.get();
    return std::make_shared<ResponseRewriteGlobalConfig>(
        proto_config, std::move(decompressor_gzip_factory), std::move(decompressor_brotli_factory),
        std::move(compressor_gzip_factory), std::move(compressor_brotli_factory));
  }
  void setup(const std::string& yaml, const std::string& yaml1,
             const Http::TestRequestHeaderMapImpl& res_headers) {
    v3::ResponseRewritePerRoute per_route_proto;
    TestUtility::loadFromYaml(yaml, per_route_proto);
    config_ = std::make_shared<ResponseRewriteRouteConfig>(per_route_proto);
    headers_ = res_headers;
    route_ = std::make_shared<NiceMock<Router::MockRoute>>();
    filter_ = std::make_unique<ResponseRewrite>(setupConfig(yaml1), context_);
    filter_->setEncoderFilterCallbacks(encoder_callbacks_);

    ON_CALL(encoder_callbacks_, route()).WillByDefault(Return(route_));
    ON_CALL(*route_, mostSpecificPerFilterConfig(FILTER_NAME)).WillByDefault(Return(config_.get()));
  }

  void setup_request(const std::string& yaml, const std::string& yaml1,
                     const Http::TestRequestHeaderMapImpl& res_headers) {
    v3::ResponseRewritePerRoute per_route_proto;
    TestUtility::loadFromYaml(yaml, per_route_proto);
    config_ = std::make_shared<ResponseRewriteRouteConfig>(per_route_proto);
    request_headers_ = res_headers;
    route_ = std::make_shared<NiceMock<Router::MockRoute>>();
    filter_ = std::make_unique<ResponseRewrite>(setupConfig(yaml1), context_);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);

    ON_CALL(decoder_callbacks_, route()).WillByDefault(Return(route_));
    ON_CALL(*route_, mostSpecificPerFilterConfig(FILTER_NAME)).WillByDefault(Return(config_.get()));
  }

  void setAddressToReturn(const std::string& address) {
    callbacks_.stream_info_.downstream_connection_info_provider_->setRemoteAddress(
        Network::Utility::resolveUrl(address));
  }

  Compression::Decompressor::MockDecompressorFactory* decompressor_factory_gzip_{};
  Compression::Decompressor::MockDecompressorFactory* decompressor_factory_brotli_{};
  Compression::Compressor::MockCompressorFactory* compressor_factory_gzip_{};
  Compression::Compressor::MockCompressorFactory* compressor_factory_brotli_{};
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_callbacks_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Server::Configuration::MockServerFactoryContext> context_;
  std::shared_ptr<NiceMock<Router::MockRoute>> route_;
  std::shared_ptr<ResponseRewriteRouteConfig> config_;
  std::unique_ptr<ResponseRewrite> filter_;
  Http::TestResponseHeaderMapImpl headers_;
  Http::TestRequestHeaderMapImpl request_headers_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks_;
  Buffer::OwnedImpl data_;
  static std::string filter_name_;
  const std::string yaml_global = R"EOF(
    decompressor_library_gzip:
      name: gzip
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.gzip.decompressor.v3.Gzip
    decompressor_library_brotli:
      name: br
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.brotli.decompressor.v3.Brotli
    compressor_library_gzip:
      name: gzip1
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip
    compressor_library_brotli:
      name: br1
      typed_config:
        "@type": type.googleapis.com/envoy.extensions.compression.brotli.compressor.v3.Brotli
)EOF";
};
std::string ResponseRewriteTest::filter_name_(FILTER_NAME);
TEST_F(ResponseRewriteTest, compressor) {
  auto config = setupConfig(yaml_global);
  auto compressor_gzip = config->makeCompressorGzip();
  auto compressor_Brotli = config->makeCompressorBrotli();
  auto decompressor_gzip = config->makeDecompressorGzip();
  auto decompressor_Brotli = config->makeDecompressorBrotli();
}

TEST_F(ResponseRewriteTest, encodeheader) {
  const std::string yaml = R"(
    enable: true
    status_codes: 200
    rules:
      - id: 1
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "number"
          data_content:
        rewrite_rule:
          Sensitive_type: "phone"
          shuffle_rewrite:
        enable: true
    compressor_enable: true
    buffer: 10000
  )";

  setup(yaml, yaml_global, response_headers);
  ResponseRewriteFilterConfigFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);
  NiceMock<Server::Configuration::MockServerFactoryContext> context;

  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, context, ProtobufMessage::getNullValidationVisitor());
  const auto* config = dynamic_cast<const ResponseRewriteRouteConfig*>(route_config.get());
  filter_->filter_config_ = config;
  EXPECT_TRUE(filter_->filter_config_->compressor_enable());

  // // stream =ture
  EXPECT_EQ(filter_->encodeHeaders(headers_, true), Http::FilterHeadersStatus::Continue);
  {
    EXPECT_EQ(filter_->encodeHeaders(headers_, false), Http::FilterHeadersStatus::StopIteration);
  }
}

TEST_F(ResponseRewriteTest, decodedata) {
  const std::string yaml = R"(
    enable: true
    status_codes: 200
    rules:
      - id: 1
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "number"
          data_content:
        rewrite_rule:
          Sensitive_type: "phone"
          shuffle_rewrite:
        enable: true
    compressor_enable: true
    buffer: 10000
  )";

  setup(yaml, yaml_global, request_headers);
  ResponseRewriteFilterConfigFactory factory;
  ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
  TestUtility::loadFromYaml(yaml, *proto_config);
  NiceMock<Server::Configuration::MockServerFactoryContext> context;

  const auto route_config = factory.createRouteSpecificFilterConfig(
      *proto_config, context, ProtobufMessage::getNullValidationVisitor());
  const auto* config = dynamic_cast<const ResponseRewriteRouteConfig*>(route_config.get());
  EXPECT_TRUE(config->compressor_enable());
  // 白名单测试
  {
    setAddressToReturn("tcp://127.0.0.1:80");
    const std::string yaml = R"(
    enable: true
    status_codes: 200
    rules:
      - id: 1
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "number"
          data_content:
        rewrite_rule:
          Sensitive_type: "phone"
          shuffle_rewrite:
        enable: true
    compressor_enable: true
    whitelist:
      ip_whitelist:
        - ip_whitelist:
            ip_list:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
          sensitive_type: "dd"
          enable: true
      user_whitelist:
        - user:
            user_name: "ouryun"
            case_sensitive: false
            invert: false
          sensitive_type: "dd"
          enable: true
    buffer: 10000
  )";
    setup(yaml, yaml_global, request_headers);
    ResponseRewriteFilterConfigFactory factory;
    ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
    TestUtility::loadFromYaml(yaml, *proto_config);
    NiceMock<Server::Configuration::MockServerFactoryContext> context;

    const auto route_config = factory.createRouteSpecificFilterConfig(
        *proto_config, context, ProtobufMessage::getNullValidationVisitor());
    const auto* config = dynamic_cast<const ResponseRewriteRouteConfig*>(route_config.get());
    auto while_list = config->whiteList();
    auto ip_while_list = while_list.ipWhiteListPtr();
    auto user_whitelist = while_list.userWhiteListPtr();
    for (auto& ip : ip_while_list) {
      EXPECT_TRUE(ip->matchIp(
          callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress()));
    }
    for (auto& user : user_whitelist) {
      EXPECT_TRUE(user->matchUsername("ouryun"));
    }
  }
  // 规则测试
  {
    setAddressToReturn("tcp://127.0.0.1:80");
    const std::string yaml = R"(
    enable: true
    status_codes: 200
    rules:
      - id: 1
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        enable: true
    compressor_enable: true
  )";
    setup(yaml, yaml_global, request_headers);
    ResponseRewriteFilterConfigFactory factory;
    ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
    TestUtility::loadFromYaml(yaml, *proto_config);
    NiceMock<Server::Configuration::MockServerFactoryContext> context;

    const auto route_config = factory.createRouteSpecificFilterConfig(
        *proto_config, context, ProtobufMessage::getNullValidationVisitor());
    const auto* config = dynamic_cast<const ResponseRewriteRouteConfig*>(route_config.get());

    const auto& rules = config->rules();
    for (size_t id = 0; id < rules.size(); id++) {
      EXPECT_TRUE(rules[id]->match(
          callbacks_.stream_info_.downstream_connection_info_provider_->remoteAddress(),
          request_headers, ""));
    }
  }
}

TEST_F(ResponseRewriteTest, encodedata) {
  // 洗牌
  {
    setAddressToReturn("tcp://127.0.0.1:80");
    const std::string yaml = R"(
    enable: true
    status_codes: 200
    rules:
      - id: 1
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "number"
          data_content:
        rewrite_rule:
          Sensitive_type: "phone"
          shuffle_rewrite:
        enable: true
    compressor_enable: true
    buffer: 10000
  )";
    setup_request(yaml, yaml_global, request_headers);
    Buffer::OwnedImpl data{"{\"number\":\"1234567890\"}"};
    std::string_view data_view(static_cast<char*>(data.linearize(data.length())), data.length());
    ResponseRewriteFilterConfigFactory factory;
    ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
    TestUtility::loadFromYaml(yaml, *proto_config);
    NiceMock<Server::Configuration::MockServerFactoryContext> context;
    const auto route_config = factory.createRouteSpecificFilterConfig(
        *proto_config, context, ProtobufMessage::getNullValidationVisitor());
    const auto* config = dynamic_cast<const ResponseRewriteRouteConfig*>(route_config.get());
    filter_->filter_config_ = config;
    EXPECT_EQ(filter_->decodeHeaders(request_headers_, false), Http::FilterHeadersStatus::Continue);
    auto regex_matcher = config->matcher();
    std::vector<RegexMatcher::MatchResult> record;
    regex_matcher->match(data_view, record, RegexMatcher::EncodingType::UTF8, 0);
    Buffer::OwnedImpl data_;
    filter_->doRewrite(record, data_view, data_);
    EXPECT_NE(data_.toString(), "{\"number\":\"1234567890\"}");
  }

  // 变换
  {
    setAddressToReturn("tcp://127.0.0.1:80");
    const std::string yaml = R"(
    enable: true
    status_codes: 200
    rules:
      - id: 1
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "字符位移"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "char"
          data_content:
        rewrite_rule:
          transform_rewrite:
            transform_type: CHARACTER_SHIFT
            character_shift:
              direction: LEFT
              shift_amount: 2
        enable: true
      - id: 2
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "日期取整"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "date"
          data_content:
        rewrite_rule:
          transform_rewrite:
            transform_type: DATE_ROUND
            date_round:
              level: YEAR
        enable: true
      - id: 3
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "数字取整"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "number"
          data_content:
        rewrite_rule:
          transform_rewrite:
            transform_type: NUMERIC_ROUND
            numeric_round:
              decimal_places: 2
        enable: true
    compressor_enable: true
    buffer: 10000
  )";
    setup_request(yaml, yaml_global, request_headers);
    Buffer::OwnedImpl data{
        "{\"number\":\"3456.12\"},{\"date\":\"2024-01-01 15:15:30\"},{\"char\":\"345678\"}"};
    std::string_view data_view(static_cast<char*>(data.linearize(data.length())), data.length());
    ResponseRewriteFilterConfigFactory factory;
    ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
    TestUtility::loadFromYaml(yaml, *proto_config);
    NiceMock<Server::Configuration::MockServerFactoryContext> context;
    const auto route_config = factory.createRouteSpecificFilterConfig(
        *proto_config, context, ProtobufMessage::getNullValidationVisitor());
    const auto* config = dynamic_cast<const ResponseRewriteRouteConfig*>(route_config.get());
    filter_->filter_config_ = config;
    EXPECT_EQ(filter_->decodeHeaders(request_headers_, false), Http::FilterHeadersStatus::Continue);
    auto regex_matcher = config->matcher();
    std::vector<RegexMatcher::MatchResult> record;
    regex_matcher->match(data_view, record, RegexMatcher::EncodingType::UTF8, 0);
    Buffer::OwnedImpl data_;
    filter_->doRewrite(record, data_view, data_);
    EXPECT_EQ(data_.toString(),
              "{\"number\":\"3450\"},{\"date\":\"2024-00-00 00:00:00\"},{\"char\":\"783456\"}");
  }
  // 遮盖
  {
    setAddressToReturn("tcp://127.0.0.1:80");
    const std::string yaml = R"(
    enable: true
    status_codes: 200
    rules:
      - id: 1
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "all"
          data_content:
        rewrite_rule:
          Sensitive_type: "shl"
          cover_rewrite:
            cover_mode: COVER_ALL
            cover_character: "*"
            cover_type: COVER
        enable: true
      - id: 2
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "11"
          data_content:
        rewrite_rule:
          Sensitive_type: "shl"
          cover_rewrite:
            cover_mode: RESERVE_FIRST1_LAST1
            cover_character: "*"
            cover_type: COVER
        enable: true
      - id: 3
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "32"
          data_content:
        rewrite_rule:
          Sensitive_type: "shl"
          cover_rewrite:
            cover_mode: RESERVE_FIRST3_LAST2
            cover_character: "*"
            cover_type: COVER
        enable: true
      - id: 4
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "34"
          data_content:
        rewrite_rule:
          Sensitive_type: "shl"
          cover_rewrite:
            cover_mode: RESERVE_FIRST3_LAST4
            cover_character: "*"
            cover_type: COVER
        enable: true
      - id: 5
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "COVER"
          data_content:
        rewrite_rule:
          Sensitive_type: "shl"
          cover_rewrite:
            cover_mode: CUSTOM
            rules:
              - start: 1
                end: 1
              - start: 4
                end: 10
            cover_character: "*"
            cover_type: COVER
        enable: true
      - id: 6
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "RESERVE"
          data_content:
        rewrite_rule:
          Sensitive_type: "shl"
          cover_rewrite:
            cover_mode: CUSTOM
            rules:
              - start: 1
                end: 1
              - start: 4
                end: 10
            cover_character: "*"
            cover_type: RESERVE
        enable: true
    compressor_enable: true
    buffer: 10000
  )";
    setup_request(yaml, yaml_global, request_headers);
    Buffer::OwnedImpl data{
        "{\"all\":\"1234567890\"},{\"11\":\"1234567890\"},{\"32\":\"1234567890\"},{"
        "\"34\":\"1234567890\"},{\"COVER\":\"1234567890\"},{\"RESERVE\":\"1234567890\"}"};
    std::string_view data_view(static_cast<char*>(data.linearize(data.length())), data.length());
    ResponseRewriteFilterConfigFactory factory;
    ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
    TestUtility::loadFromYaml(yaml, *proto_config);
    NiceMock<Server::Configuration::MockServerFactoryContext> context;
    const auto route_config = factory.createRouteSpecificFilterConfig(
        *proto_config, context, ProtobufMessage::getNullValidationVisitor());
    const auto* config = dynamic_cast<const ResponseRewriteRouteConfig*>(route_config.get());
    filter_->filter_config_ = config;
    EXPECT_EQ(filter_->decodeHeaders(request_headers_, false), Http::FilterHeadersStatus::Continue);
    auto regex_matcher = config->matcher();
    std::vector<RegexMatcher::MatchResult> record;
    regex_matcher->match(data_view, record, RegexMatcher::EncodingType::UTF8, 0);
    Buffer::OwnedImpl data_;
    filter_->doRewrite(record, data_view, data_);
    EXPECT_EQ(data_.toString(),
              "{\"all\":\"**********\"},{\"11\":\"1********0\"},{\"32\":\"123*****90\"},{"
              "\"34\":\"123***7890\"},{\"COVER\":\"*23*******\"},{\"RESERVE\":\"1**4567890\"}");
  }

  // 替换
  {
    setAddressToReturn("tcp://127.0.0.1:80");
    const std::string yaml = R"(
    enable: true
    status_codes: 200
    rules:
      - id: 1
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "all"
          data_content:
        rewrite_rule:
          Sensitive_type: "11"
          replace_rewrite:
            rule_type: REGEX_MATCH
            regex_match:
              regex: "广东省"
            value_type: FIXED_VALUE
            replace_value: "A"
        enable: true
      - id: 2
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "11"
          data_content:
        rewrite_rule:
          Sensitive_type: "11"
          replace_rewrite:
            rule_type: REPLACE_ALL
            replace:
            value_type: FIXED_VALUE
            replace_value: "*"
        enable: true
      - id: 3
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "32"
          data_content:
        rewrite_rule:
          Sensitive_type: "11"
          replace_rewrite:
            rule_type: REPLACE_FIRST3
            replace:
            value_type: RANDOM_VALUE
            replace_value: "*"
        enable: true
      - id: 4
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "34"
          data_content:
        rewrite_rule:
          Sensitive_type: "11"
          replace_rewrite:
            rule_type: REPLACE_LAST4
            replace:
            value_type: SAMPLE_VALUE
            replace_value: "*"
        enable: true
      - id: 5
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "COVER"
          data_content:
        rewrite_rule:
          Sensitive_type: "shl"
          replace_rewrite:
            rule_type: CUSTOM
            custom:
              rules:
                - start: 1
                  end: 1
                - start: 4
                  end: 10
              cover_type: COVER
            value_type: RANDOM_VALUE
            replace_value: "*"
        enable: true
      - id: 6
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "RESERVE"
          data_content:
        rewrite_rule:
          Sensitive_type: "shl"
          replace_rewrite:
            rule_type: CUSTOM
            custom:
              rules:
                - start: 1
                  end: 1
                - start: 4
                  end: 10
              cover_type: RESERVE
            replace_value: "*"
            value_type: FIXED_VALUE
        enable: true
    compressor_enable: true
    buffer: 10000
  )";
    setup_request(yaml, yaml_global, request_headers);
    Buffer::OwnedImpl data{
        "{\"all\":\"广东省4567890\"},{\"11\":\"1234567890\"},{\"32\":\"1234567890\"},{"
        "\"34\":\"1234567890\"},{\"COVER\":\"1234567890\"},{\"RESERVE\":\"1234567890\"}"};
    std::string_view data_view(static_cast<char*>(data.linearize(data.length())), data.length());
    ResponseRewriteFilterConfigFactory factory;
    ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
    TestUtility::loadFromYaml(yaml, *proto_config);
    NiceMock<Server::Configuration::MockServerFactoryContext> context;
    const auto route_config = factory.createRouteSpecificFilterConfig(
        *proto_config, context, ProtobufMessage::getNullValidationVisitor());
    const auto* config = dynamic_cast<const ResponseRewriteRouteConfig*>(route_config.get());
    filter_->filter_config_ = config;
    EXPECT_EQ(filter_->decodeHeaders(request_headers_, false), Http::FilterHeadersStatus::Continue);
    auto regex_matcher = config->matcher();
    std::vector<RegexMatcher::MatchResult> record;
    regex_matcher->match(data_view, record, RegexMatcher::EncodingType::UTF8, 0);
    Buffer::OwnedImpl data_;
    filter_->doRewrite(record, data_view, data_);
    EXPECT_EQ(data_.toString(),
              "{\"all\":\"A4567890\"},{\"11\":\"*\"},{\"32\":\"***4567890\"},{"
              "\"34\":\"123456*\"},{\"COVER\":\"*23*******\"},{\"RESERVE\":\"1*4567890\"}");
  }

  // 哈希加密
  {
    setAddressToReturn("tcp://127.0.0.1:80");
    const std::string yaml = R"(
    enable: true
    status_codes: 200
    rules:
      - id: 1
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "1,12"
          data_content:
        rewrite_rule:
          Sensitive_type: "phone"
          hash_encrypt_rewrite:
            encryption_algorithm: MD5
            salt_value: "123456"
        enable: true
      - id: 2
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: DATA_CONTENT_AND_FIELD_NAME
          recognition_logic: SATISFY_ALL
          field_name: "2"
          data_content: "1234567890"
        rewrite_rule:
          Sensitive_type: "phone"
          hash_encrypt_rewrite:
            encryption_algorithm: SHA256
            salt_value: "123456"
        enable: true
      - id: 3
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "3"
          data_content:
        rewrite_rule:
          Sensitive_type: "phone"
          hash_encrypt_rewrite:
            encryption_algorithm: SHA512
            salt_value: "123456"
        enable: true
      - id: 4
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "4"
          data_content:
        rewrite_rule:
          Sensitive_type: "phone"
          hash_encrypt_rewrite:
            encryption_algorithm: SM3
            salt_value: "123456"
        enable: true
    compressor_enable: true
    buffer: 10000
  )";
    setup_request(yaml, yaml_global, request_headers);
    Buffer::OwnedImpl data{
        "{\"1\":\"1234567890\",\"2\":\"1234567890\",\"3\":\"1234567890\",\"4\":\"1234567890\"}"};
    std::string_view data_view(static_cast<char*>(data.linearize(data.length())), data.length());
    ResponseRewriteFilterConfigFactory factory;
    ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
    TestUtility::loadFromYaml(yaml, *proto_config);
    NiceMock<Server::Configuration::MockServerFactoryContext> context;
    const auto route_config = factory.createRouteSpecificFilterConfig(
        *proto_config, context, ProtobufMessage::getNullValidationVisitor());
    const auto* config = dynamic_cast<const ResponseRewriteRouteConfig*>(route_config.get());
    filter_->filter_config_ = config;
    EXPECT_EQ(filter_->decodeHeaders(request_headers_, false), Http::FilterHeadersStatus::Continue);
    auto regex_matcher = config->matcher();
    std::vector<RegexMatcher::MatchResult> record;
    regex_matcher->match(data_view, record, RegexMatcher::EncodingType::UTF8, 0);
    Buffer::OwnedImpl data_;
    filter_->doRewrite(record, data_view, data_);
    EXPECT_NE(
        data_.toString(),
        "{\"1\":\"1234567890\",\"2\":\"1234567890\",\"3\":\"1234567890\",\"4\":\"1234567890\"}");
  }

  // 加密算法
  {
    setAddressToReturn("tcp://127.0.0.1:80");
    const std::string yaml = R"(
    enable: true
    status_codes: 200
    rules:
      - id: 1
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "1"
          data_content:
        rewrite_rule:
          Sensitive_type: "phone"
          encrypt_algorithm_rewrite:
            encryption_algorithm: DES
            encryption_key: "12345678"
        enable: true
      - id: 2
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "2"
          data_content:
        rewrite_rule:
          Sensitive_type: "phone"
          encrypt_algorithm_rewrite:
            encryption_algorithm: TRIPLE_DES
            encryption_key: "1234567890abcdef12345678"
        enable: true
      - id: 3
        match:
          ip_list:
            ip_set:
              list:
                address_prefix: 127.0.0.1
                prefix_len: 32
            invert: false
        identify_rule:
          sensitive_type: "号码"
          identify_type: FIELD_NAME
          recognition_logic: SATISFY_ANY
          field_name: "3"
          data_content:
        rewrite_rule:
          Sensitive_type: "phone"
          encrypt_algorithm_rewrite:
            encryption_algorithm: AES
            encryption_key: "0123456789abcdef"
        enable: true
    compressor_enable: true
    buffer: 10000
  )";
    setup_request(yaml, yaml_global, request_headers);
    Buffer::OwnedImpl data{
        "{\"1\":\"1234567890\",\"2\":\"1234567890\",\"3\":\"1234567890\",\"4\":\"1234567890\"}"};
    std::string_view data_view(static_cast<char*>(data.linearize(data.length())), data.length());
    ResponseRewriteFilterConfigFactory factory;
    ProtobufTypes::MessagePtr proto_config = factory.createEmptyRouteConfigProto();
    TestUtility::loadFromYaml(yaml, *proto_config);
    NiceMock<Server::Configuration::MockServerFactoryContext> context;
    const auto route_config = factory.createRouteSpecificFilterConfig(
        *proto_config, context, ProtobufMessage::getNullValidationVisitor());
    const auto* config = dynamic_cast<const ResponseRewriteRouteConfig*>(route_config.get());
    filter_->filter_config_ = config;
    EXPECT_EQ(filter_->decodeHeaders(request_headers_, false), Http::FilterHeadersStatus::Continue);
    auto regex_matcher = config->matcher();
    std::vector<RegexMatcher::MatchResult> record;
    regex_matcher->match(data_view, record, RegexMatcher::EncodingType::UTF8, 0);
    Buffer::OwnedImpl data_;
    filter_->doRewrite(record, data_view, data_);
    EXPECT_NE(
        data_.toString(),
        "{\"1\":\"1234567890\",\"2\":\"1234567890\",\"3\":\"1234567890\",\"4\":\"1234567890\"}");
  }
}

} // namespace ResponseRewrite
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy