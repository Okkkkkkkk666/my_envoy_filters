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
#include "test/test_common/utility.h"

#include "filters/api/envoy/extensions/filters/http/waf/v3/waf.pb.h"
#include "filters/source/extensions/filters/http/waf/waf.h"
#include "filters/source/extensions/filters/http/waf/config.h"

using testing::InSequence;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WafFilter {

namespace v3 = envoy::extensions::filters::http::waf::v3;

static const std::string yaml_filter = R"(
  paranoia_level: L4
  mode: precise
  rule_types: 134209535
  detection_only: false
  )";

// static const std::string yaml_per_filter = R"(
//     "@type": type.googleapis.com/envoy.extensions.filters.http.waf.v3.WafRoute
//     enable: true
//     dryrun: false
//     waf_ratelimit: true
//     rules:
//       - condition:
//         - prefix: /
//           case_sensitive: true
//         action:
//           quotas:
//             - duration: 60
//               max_count: 10
//           target: IP_AND_PATH
//           add_headers: true
//     ip_whitelist:
//       - ip_range_list:
//           - start_ip: 192.192.100.1
//             end_ip: 192.192.100.200
//         ip_list:
//           list:
//           - address_prefix: 172.16.0.253
//             prefix_len: 32
//         enable: true
//   )";

class WafFilterTest : public testing::Test {
public:
  WafFilterTest() = default;

  void SetUp() override {
    setup(fmt::format(yaml_filter));
    stream_info_.downstream_connection_info_provider_->setRemoteAddress(
        std::make_unique<Network::Address::Ipv4Instance>("127.0.0.1", 5678));
  }

  void setup(const std::string& yaml) {
    ON_CALL(context_, threadLocal()).WillByDefault(ReturnRef(thread_local_));
    v3::WafGlobal global_config;
    if (!yaml.empty()) {
      TestUtility::loadFromYaml(yaml, global_config);
    }
    global_config_ =
        std::make_shared<FilterGlobalConfig>(global_config, stats_prefix_, factory_context_);
    filter_ = std::make_shared<Filter>(global_config_, context_);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    filter_->setEncoderFilterCallbacks(encoder_callbacks_);
    stream_info_.downstream_connection_info_provider_->setRemoteAddress(
        std::make_unique<Network::Address::Ipv4Instance>("127.0.0.1", 5678));
  }

  std::string stats_prefix_{"envoy.waf"};
  std::shared_ptr<FilterGlobalConfig> global_config_;

  std::shared_ptr<Filter> filter_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_callbacks_;
  NiceMock<Server::Configuration::MockServerFactoryContext> context_;
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;
  NiceMock<ThreadLocal::MockInstance> thread_local_;
  NiceMock<Network::MockConnection> connection_{};
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info_;
};

// 验证日志格式，正常请求被放行
TEST_F(WafFilterTest, LogFormatNormal) {

  Http::TestRequestHeaderMapImpl normal_headers{
      {":authority", "127.0.0.1:10010"},
      {":path", "/home"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "PostmanRuntime/7.29.2"},
      {"accept", "*/*"},
      {"postman-token", "a601015f-868b-49cb-a045-f64b63e450f8"},
      {"accept-encoding", "gzip, deflate, br"},
      {"cookie", "ABTEST=1|1676891903|v17; Cookie_2=value; cookies=value"},
      {"x-forwarded-for", "192.168.137.251"},
      {"x-forwarded-proto", "http"},
      {"stone_rhino-internal", "true"},
      {"x-request-id", "86c4f465-ca3e-48ff-ae06-b4a63dcd29fa"},
      {"stone_rhino-expected-rq-timeout-ms", "15000"},
  };
  EXPECT_CALL(decoder_callbacks_, streamInfo()).Times(4).WillRepeatedly(ReturnRef(stream_info_));
  EXPECT_CALL(decoder_callbacks_, connection()).Times(2).WillRepeatedly(Return(&connection_));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(normal_headers, true));
  filter_->onStreamComplete();

  auto log_json = MessageUtil::getJsonStringFromMessageOrDie(filter_->getLog());
  std::cout << log_json << std::endl;
  // {"method":"GET","path":"/home","downstream":"127.0.0.1"}
  Envoy::Json::ObjectSharedPtr log_json_object = Envoy::Json::Factory::loadFromString(log_json);

  // refuse默认false，protobuf转json时，json中不会有该字段
  EXPECT_EQ(log_json_object->hasObject("refuse"), false);
  EXPECT_EQ(log_json_object->hasObject("filenames"), false);
  EXPECT_EQ(log_json_object->hasObject("ratelimit_refuse"), false);
  EXPECT_EQ(log_json_object->hasObject("rule_id"), false);
  EXPECT_EQ(log_json_object->hasObject("quota"), false);
  EXPECT_EQ(log_json_object->getString("method"), "GET");
  EXPECT_EQ(log_json_object->getString("path"), "/home");
  EXPECT_EQ(log_json_object->hasObject("param"), false);
  EXPECT_EQ(log_json_object->getString("downstream"), "127.0.0.1");
}

// 验证日志格式，异常请求被拦截
TEST_F(WafFilterTest, LogFormatXSS) {

  Http::TestRequestHeaderMapImpl xss_headers{
      {":authority", "127.0.0.1:10010"},
      {":path", "/%3Cscript%3Ealert(1)%3C/script%3E"},
      {":method", "GET"},
      {"user-agent", "PostmanRuntime/7.29.2"},
      {"accept", "*/*"},
      {"postman-token", "5b1a7005-1c0f-410e-8110-8df8dc1dd488"},
      {"accept-encoding", "gzip, deflate, br"},
      {"connection", "keep-alive"},
      {"cookie", "ABTEST=1|1676891903|v17; Cookie_2=value; cookies=value"},
  };
  EXPECT_CALL(decoder_callbacks_, streamInfo()).Times(4).WillRepeatedly(ReturnRef(stream_info_));
  EXPECT_CALL(decoder_callbacks_, connection()).Times(2).WillRepeatedly(Return(&connection_));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(xss_headers, true));
  filter_->onStreamComplete();

  auto log_json = MessageUtil::getJsonStringFromMessageOrDie(filter_->getLog());
  std::cout << log_json << std::endl;
  // {"refuse":true,"method":"GET","path":"/%3Cscript%3Ealert(1)%3C/script%3E","downstream":"127.0.0.1","filenames":"514"}
  Envoy::Json::ObjectSharedPtr log_json_object = Envoy::Json::Factory::loadFromString(log_json);
  EXPECT_EQ(log_json_object->getBoolean("refuse"), true);
  EXPECT_EQ(log_json_object->getString("method"), "GET");
  EXPECT_EQ(log_json_object->getString("path"), "/%3Cscript%3Ealert(1)%3C/script%3E");
  EXPECT_EQ(log_json_object->hasObject("param"), false);
  EXPECT_EQ(log_json_object->getString("downstream"), "127.0.0.1");
  EXPECT_EQ(log_json_object->getString("filenames"), "512");
  EXPECT_EQ(log_json_object->hasObject("param"), false);
  EXPECT_EQ(log_json_object->hasObject("rule_id"), true);
  EXPECT_EQ(log_json_object->hasObject("ratelimit_refuse"), false);
  EXPECT_EQ(log_json_object->hasObject("quota"), false);
}

// 请求查询参数攻击拦截
TEST_F(WafFilterTest, RequestParamsDetect) {
  auto modsec_rules = global_config_->modsec_rules();

  const std::string xss_rule = R"(
    SecRule ARGS "@rx (?i)<script[^>]*>[\s\S]*?" \
    "id:99000,\
    phase:1,\
    block,\
    capture,\
    t:none,t:utf8toUnicode,t:urlDecodeUni,t:htmlEntityDecode,t:jsDecode,t:cssDecode,t:removeNulls,\
    msg:'XSS Filter - Category 1: Script Tag Vector',\
    logdata:'Matched Data: %{TX.0} found within %{MATCHED_VAR_NAME}: %{MATCHED_VAR}',\
    tag:'application-multi',\
    tag:'language-multi',\
    tag:'platform-multi',\
    tag:'attack-xss',\
    tag:'paranoia-level/1',\
    tag:'OWASP_CRS',\
    tag:'capec/1000/152/242',\
    ver:'OWASP_CRS/4.0.0-rc1',\
    severity:'CRITICAL',\
    setvar:'tx.xss_score=+%{tx.critical_anomaly_score}',\
    setvar:'tx.inbound_anomaly_score_pl1=+%{tx.critical_anomaly_score}'"
  )"; // ARGS: 对查询参数进行检测，phase:1 请求头阶段
  int num = modsec_rules->load(xss_rule.c_str());
  EXPECT_EQ(num, 1);

  Http::TestRequestHeaderMapImpl xss_headers{
      {":path", "/%3Cscript%3Ealert(1)%3C/script%3E"},
  };
  EXPECT_CALL(decoder_callbacks_, streamInfo()).Times(4).WillRepeatedly(ReturnRef(stream_info_));
  EXPECT_CALL(decoder_callbacks_, connection()).Times(2).WillRepeatedly(Return(&connection_));
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::Forbidden, _, _, _, _)).Times(1);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(xss_headers, true));
}

// 请求头攻击拦截
TEST_F(WafFilterTest, RequestHeadersDetect) {
  auto modsec_rules = global_config_->modsec_rules();

  const std::string xss_rule = R"(
    SecRule REQUEST_HEADERS "@rx (?i)<script[^>]*>[\s\S]*?" \
    "id:99000,\
    phase:1,\
    block,\
    capture,\
    t:none,t:utf8toUnicode,t:urlDecodeUni,t:htmlEntityDecode,t:jsDecode,t:cssDecode,t:removeNulls,\
    msg:'XSS Filter - Category 1: Script Tag Vector',\
    logdata:'Matched Data: %{TX.0} found within %{MATCHED_VAR_NAME}: %{MATCHED_VAR}',\
    tag:'application-multi',\
    tag:'language-multi',\
    tag:'platform-multi',\
    tag:'attack-xss',\
    tag:'paranoia-level/1',\
    tag:'OWASP_CRS',\
    tag:'capec/1000/152/242',\
    ver:'OWASP_CRS/4.0.0-rc1',\
    severity:'CRITICAL',\
    setvar:'tx.xss_score=+%{tx.critical_anomaly_score}',\
    setvar:'tx.inbound_anomaly_score_pl1=+%{tx.critical_anomaly_score}'"
  )"; // ARGS: 对请求头进行检测，phase:1 请求头阶段
  int num = modsec_rules->load(xss_rule.c_str());
  EXPECT_EQ(num, 1);

  Http::TestRequestHeaderMapImpl xss_headers{
      {"envoy_data", "/%3Cscript%3Ealert(1)%3C/script%3E"},
  };
  EXPECT_CALL(decoder_callbacks_, streamInfo()).Times(4).WillRepeatedly(ReturnRef(stream_info_));
  EXPECT_CALL(decoder_callbacks_, connection()).Times(2).WillRepeatedly(Return(&connection_));
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::Forbidden, _, _, _, _)).Times(1);

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(xss_headers, true));
}

// 请求体攻击拦截
TEST_F(WafFilterTest, RequestBodyDetect) {
  Http::TestRequestHeaderMapImpl xss_headers{
      {":authority", "127.0.0.1:10010"},
      {":path", "/"},
      {":method", "POST"},
      {"content-type", "application/json"},
      {"user-agent", "PostmanRuntime/7.29.2"},
      {"accept", "*/*"},
      {"postman-token", "d6552435-375a-4935-9882-b08a9e17af77"},
      {"accept-encoding", "gzip, deflate, br"},
      {"connection", "keep-alive"},
      {"content-length", "37"},
      {"cookie", "ABTEST=1|1676891903|v17; Cookie_2=value; cookies=value"},
  };
  EXPECT_CALL(decoder_callbacks_, streamInfo()).Times(4).WillRepeatedly(ReturnRef(stream_info_));
  EXPECT_CALL(decoder_callbacks_, connection()).Times(2).WillRepeatedly(Return(&connection_));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(xss_headers, false));

  Buffer::OwnedImpl xss_buffer("{\"data\":\"<script>alert(1);</script>\"}");

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::Forbidden, _, _, _, _)).Times(1);
  EXPECT_EQ(Http::FilterDataStatus::StopIterationNoBuffer, filter_->decodeData(xss_buffer, true));
}

// 响应头数据泄漏拦截
TEST_F(WafFilterTest, ResponseHeadersDetect) {
  Http::TestRequestHeaderMapImpl request_header{
      {":authority", "127.0.0.1:10010"},
      {":path", "/"},
      {":method", "POST"},
      {"content-type", "application/json"},
      {"user-agent", "PostmanRuntime/7.29.2"},
      {"accept", "*/*"},
      {"postman-token", "d6552435-375a-4935-9882-b08a9e17af77"},
      {"accept-encoding", "gzip, deflate, br"},
      {"connection", "keep-alive"},
      {"content-length", "37"},
      {"cookie", "ABTEST=1|1676891903|v17; Cookie_2=value; cookies=value"},
  };
  Buffer::OwnedImpl request_boty("{\"data\":\"aacriptaalertaaaaaascripaa\"}");
  EXPECT_CALL(decoder_callbacks_, streamInfo()).Times(4).WillRepeatedly(ReturnRef(stream_info_));
  EXPECT_CALL(decoder_callbacks_, connection()).Times(2).WillRepeatedly(Return(&connection_));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(request_header, false));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(request_boty, true));
  Http::TestResponseHeaderMapImpl server_error_headers{
      {":status", "500"},
  }; // 5xx的错误信息不会返回给客户，防止信息泄漏
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::Forbidden, _, _, _, _)).Times(1);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->encodeHeaders(server_error_headers, false));
}

// 响应体数据泄漏拦截
TEST_F(WafFilterTest, ResponseBodyDetect) {
  Http::TestRequestHeaderMapImpl xss_headers{
      {":authority", "127.0.0.1:10010"},
      {":path", "/"},
      {":method", "POST"},
      {"content-type", "application/json"},
      {"user-agent", "PostmanRuntime/7.29.2"},
      {"accept", "*/*"},
      {"postman-token", "d6552435-375a-4935-9882-b08a9e17af77"},
      {"accept-encoding", "gzip, deflate, br"},
      {"connection", "keep-alive"},
      {"content-length", "37"},
      {"cookie", "ABTEST=1|1676891903|v17; Cookie_2=value; cookies=value"},
  };
  EXPECT_CALL(decoder_callbacks_, streamInfo()).Times(4).WillRepeatedly(ReturnRef(stream_info_));
  EXPECT_CALL(decoder_callbacks_, connection()).Times(2).WillRepeatedly(Return(&connection_));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(xss_headers, true));

  Http::TestResponseHeaderMapImpl normal_headers{
      {":status", "200"}, {"Content-Type", "text/plain"}, {"Content-Length", "17"}};
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->encodeHeaders(normal_headers, false));

  Buffer::OwnedImpl data_leak_buffer("#!/usr/bin/python"); // 源码泄漏
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::Forbidden, _, _, _, _)).Times(1);
  EXPECT_EQ(Http::FilterDataStatus::StopIterationNoBuffer,
            filter_->encodeData(data_leak_buffer, true));
}
TEST_F(WafFilterTest, CorrentReuqest) {
  Http::TestRequestHeaderMapImpl request_header{
      {":authority", "127.0.0.1:10010"},
      {":path", "/"},
      {":method", "POST"},
      {"content-type", "application/json"},
      {"user-agent", "PostmanRuntime/7.29.2"},
      {"accept", "*/*"},
      {"postman-token", "d6552435-375a-4935-9882-b08a9e17af77"},
      {"accept-encoding", "gzip, deflate, br"},
      {"connection", "keep-alive"},
      {"content-length", "30"},
      {"cookie", "ABTEST=1|1676891903|v17; Cookie_2=value; cookies=value"},
  };
  Buffer::OwnedImpl request_boty("{\"msg\":\"corrent reuqest test\"}");
  Http::TestResponseHeaderMapImpl response_header{{":status", "200"},
                                                  {"content-type", "application/json"},
                                                  {"content-length", "63"},
                                                  {"cache-control", "no-cache, max-age=0"},
                                                  {"x-content-type-options", "nosniff"},
                                                  {"date", "Wed, 01 Mar 2023 02:17:50 GMT"},
                                                  {"server", "envoy"}};
  Buffer::OwnedImpl response_boty(
      "{\"refuse\":true,\"method\":\"POST\",\"path\":\"/"
      "\",\"param\":\"?key=value\",\"downstream\":\"127.0.0.1\",\"filenames\":\"2\"}");
  EXPECT_CALL(decoder_callbacks_, streamInfo()).Times(4).WillRepeatedly(ReturnRef(stream_info_));
  EXPECT_CALL(decoder_callbacks_, connection()).Times(2).WillRepeatedly(Return(&connection_));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(request_header, false));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(request_boty, true));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->encodeHeaders(response_header, false));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(response_boty, true));
}
} // namespace WafFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
