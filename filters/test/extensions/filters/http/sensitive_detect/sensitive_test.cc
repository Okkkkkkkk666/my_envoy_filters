#include "test/mocks/server/mocks.h"
#include "test/mocks/local_info/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test/mocks/compression/decompressor/mocks.h"
#include "test/mocks/compression/compressor/mocks.h"

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

#include "filters/source/extensions/filters/http/sensitive_detect/config.h"
#include "filters/source/extensions/filters/http/sensitive_detect/sensitive_detect.h"
#include "filters/source/extensions/filters/http/common/regex_matcher/regex_matcher.h"

namespace RegexMatcher = Envoy::Extensions::Filters::Common::RegexMatcher;
using RegexMatcherPtr = std::shared_ptr<RegexMatcher::RegexMatcher>;
namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SensitiveDetect {

class SensitiveDetectTest : public testing::Test {
public:
  SensitiveDetectTest() = default;

  std::shared_ptr<SensitiveDetectGlobalConfig> makeConfig() {
    v3::SensitiveDetectGlobal proto;
    TestUtility::loadFromYamlAndValidate(yaml, proto);
    auto decompressor_gzip_factory =
        std::make_unique<NiceMock<Compression::Decompressor::MockDecompressorFactory>>();
    auto decompressor_brotli_factory =
        std::make_unique<NiceMock<Compression::Decompressor::MockDecompressorFactory>>();
    decompressor_factory_gzip_ = decompressor_gzip_factory.get();
    decompressor_factory_brotli_ = decompressor_brotli_factory.get();
    return std::make_shared<SensitiveDetectGlobalConfig>(
        proto, std::move(decompressor_gzip_factory), std::move(decompressor_brotli_factory));
  }

  re2::StringPiece transform(Buffer::OwnedImpl& data_) {
    char* line_data = static_cast<char*>(data_.linearize(data_.length()));
    size_t line_size = data_.length();
    re2::StringPiece new_data(line_data, line_size);
    return new_data;
  }

protected:
  Compression::Decompressor::MockDecompressorFactory* decompressor_factory_gzip_{};
  Compression::Decompressor::MockDecompressorFactory* decompressor_factory_brotli_{};
  const std::string yaml = R"(
      enable: true
      rules:
        - id: 1
        - id: 2
        - id: 3
        - id: 4
        - id: 5
        - id: 6
        - id: 7
        - id: 8
        - id: 9
        - id: 10
        - id: 11
        - id: 12
        - id: 13
        - id: 14

)";
  std::string text = R"({
  "IPv4": "134.231.87.27",
  "IPv6": "3e79:a603:d35b:f3c1:9ccb:19db:36aa:a9c2",
  "MAC 地址": "41:2a:12:2e:98:d4",
  "个人普通护照号码": "G1279559",
  "公务护照号码": "S63588095",
  "固定电话": "0755-95723422",
  "域名": "guo.net",
  "外交护照号码": "D33849343",
  "姓名": "罗建平",
  "性别": "男",
  "手机号号码": "14585855702",
  "日期": "2013-04-03",
  "民族": "汉族",
  "港澳通行证": "C97423997",
  "省份": "湖南省",
  "组织机构代码": "M748443369X8474994",
  "营业执照号码": 90329528,
  "身份证号码": "213000195311062642",
  "车牌号": "吉TMC9X6C",
  "车辆识别码": "4GDBVR7G156R70W3N",
  "邮政编码": "934217",
  "邮箱": "zqiu@example.com",
  "银行卡号": "6216604003121175"
})";
};

// 测试encodeheaders
TEST_F(SensitiveDetectTest, TestEncodeHeaders) {
  // 配置enable为false的情况
  const std::string yaml = R"(
      enable: false
      rules:
        - id: 1
    )";
  v3::SensitiveDetectGlobal proto;
  TestUtility::loadFromYamlAndValidate(yaml, proto);
  auto decompressor_gzip_factory =
      std::make_unique<NiceMock<Compression::Decompressor::MockDecompressorFactory>>();
  auto decompressor_brotli_factory =
      std::make_unique<NiceMock<Compression::Decompressor::MockDecompressorFactory>>();
  decompressor_factory_gzip_ = decompressor_gzip_factory.get();
  decompressor_factory_brotli_ = decompressor_brotli_factory.get();
  auto config = std::make_shared<SensitiveDetectGlobalConfig>(
      proto, std::move(decompressor_gzip_factory), std::move(decompressor_brotli_factory));
  EXPECT_EQ(config->enable(), false);
  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  SensitiveDetect filter(config, context);
  Http::TestResponseHeaderMapImpl headers;
  EXPECT_EQ(filter.encodeHeaders(headers, false), Http::FilterHeadersStatus::Continue);

  // 配置enable为true,编码格式为UTF8
  auto config_utf8 = makeConfig();
  EXPECT_EQ(config_utf8->enable(), true);
  NiceMock<Server::Configuration::MockServerFactoryContext> context_utf8;
  SensitiveDetect filter_utf8(config_utf8, context_utf8);
  Http::TestResponseHeaderMapImpl headers_utf8{
      {":authority", "localhost:8888"},
      {":path", "/"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "curl/7.68.0"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"end-user", "envoy"},
      {"content-length", "441"},
      {"content-type", "text/json; charset=utf-8"},
  };
  filter_utf8.encodeHeaders(headers_utf8, false);
  auto type = filter_utf8.encode_type_;
  EXPECT_EQ(type, RegexMatcher::EncodingType::UTF8);
  // 编码格式为GBK
  auto config_gbk = makeConfig();
  NiceMock<Server::Configuration::MockServerFactoryContext> context_gbk;
  SensitiveDetect filter_gbk(config_gbk, context_gbk);
  Http::TestResponseHeaderMapImpl headers_gbk{
      {":authority", "localhost:8888"},
      {":path", "/"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "curl/7.68.0"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"end-user", "envoy"},
      {"content-length", "441"},
      {"content-type", "text/json; charset=gbk"},
  };
  filter_gbk.encodeHeaders(headers_gbk, false);
  auto type_gbk = filter_gbk.encode_type_;
  EXPECT_EQ(type_gbk, RegexMatcher::EncodingType::GBK);

  // 编码格式为ISO
  auto config_iso = makeConfig();
  NiceMock<Server::Configuration::MockServerFactoryContext> context_iso;
  SensitiveDetect filter_iso(config_iso, context_iso);
  Http::TestResponseHeaderMapImpl headers_iso{
      {":authority", "localhost:8888"},
      {":path", "/"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "curl/7.68.0"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"end-user", "envoy"},
      {"content-length", "441"},
      {"content-type", "text/json; charset=iso"},
  };
  filter_iso.encodeHeaders(headers_iso, false);
  auto type_iso = filter_iso.encode_type_;
  EXPECT_EQ(type_iso, RegexMatcher::EncodingType::ISO);
}

// 测试encodeData
TEST_F(SensitiveDetectTest, TestEncodeData) {
  // 不是压缩编码的情况
  auto config = makeConfig();
  EXPECT_EQ(config->enable(), true);
  Buffer::OwnedImpl data_;
  data_.add(text);
  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  SensitiveDetect filter(config, context);
  filter.thread_index_ = 0; //mock thread index
  Http::TestResponseHeaderMapImpl headers{
      {":authority", "localhost:8888"},
      {":path", "/"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "curl/7.68.0"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"end-user", "envoy"},
      {"content-length", "441"},
      {"content-type", "text/json; charset=utf-8"},
  };
  filter.encodeHeaders(headers, false);
  filter.encodeData(data_, false);
  filter.onStreamComplete();
  EXPECT_EQ(filter.getLog().sensitives_size(), 14);
  ASSERT_FALSE(filter.decompressor_);
  EXPECT_EQ(filter.encodeData(data_, true), Http::FilterDataStatus::Continue);

  // 是压缩编码gzip情况
  auto config_gzip = makeConfig();
  EXPECT_EQ(config_gzip->enable(), true);
  Buffer::OwnedImpl data_gzip;
  data_gzip.add(text);
  NiceMock<Server::Configuration::MockServerFactoryContext> context_gzip;
  SensitiveDetect filter_gzip(config_gzip, context_gzip);
  filter_gzip.thread_index_ = 0; //mock thread index
  Http::TestResponseHeaderMapImpl headers_gzip{
      {":authority", "localhost:8888"},
      {":path", "/"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "curl/7.68.0"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"end-user", "envoy"},
      {"content-length", "441"},
      {"content-type", "text/json; charset=utf-8"},
      {"Content-Encoding", "gzip"},
  };
  filter_gzip.encodeHeaders(headers_gzip, false);
  filter_gzip.encodeData(data_gzip, false);
  filter_gzip.onStreamComplete();
  filter_gzip.encodeHeaders(headers_gzip, false);
  EXPECT_EQ(filter_gzip.getLog().sensitives_size(), 14);
  EXPECT_EQ(filter_gzip.encodeData(data_, true), Http::FilterDataStatus::Continue);

    // 是压缩编码br情况
  auto config_br = makeConfig();
  EXPECT_EQ(config_br->enable(), true);
  Buffer::OwnedImpl data_br;
  data_br.add(text);
  NiceMock<Server::Configuration::MockServerFactoryContext> context_br;
  SensitiveDetect filter_br(config_br, context_br);
  filter_br.thread_index_ = 0; //mock thread index
  Http::TestResponseHeaderMapImpl headers_br{
      {":authority", "localhost:8888"},
      {":path", "/"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "curl/7.68.0"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"end-user", "envoy"},
      {"content-length", "441"},
      {"content-type", "text/json; charset=utf-8"},
      {"Content-Encoding", "br"},
  };
  filter_br.encodeHeaders(headers_br, false);
  filter_br.encodeData(data_br, false);
  filter_br.onStreamComplete();
  filter_br.encodeHeaders(headers_br, false);
  EXPECT_EQ(filter_br.getLog().sensitives_size(), 14);
  EXPECT_EQ(filter_br.encodeData(data_br, true), Http::FilterDataStatus::Continue);
}

// 验证不匹配的情况
TEST_F(SensitiveDetectTest, TestNotMatch) {
  auto config = makeConfig();
  EXPECT_EQ(config->enable(), true);
  Buffer::OwnedImpl data_;
  data_.add("");
  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  SensitiveDetect filter(config, context);
  filter.thread_index_ = 0; //mock thread index
  Http::TestResponseHeaderMapImpl headers{
      {":authority", "localhost:8888"},
      {":path", "/"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "curl/7.68.0"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"end-user", "envoy"},
      {"content-length", "441"},
      {"content-type", "text/json; charset=utf-8"},
  };
  filter.encodeHeaders(headers, false);
  filter.encodeData(data_, true);
  filter.onStreamComplete();
  EXPECT_EQ(filter.getLog().sensitives_size(), 0);
}

// 测试匹配用户自定义规则
TEST_F(SensitiveDetectTest, TestAddMatch) {
  const std::string yaml = R"(
      enable: true
      rules:
        - id: 10003
          pattern: "男|女"
        - id: 10004
          pattern: "湖南省|山东省|广东省"
        - id: 10005
          pattern: "\\d{8}"
        - id: 10006
          pattern: "\\d{4}-\\d{2}-\\d{2}"
        - id: 10007
          pattern: "([0-9A-Fa-f]{1,4}:){7}[0-9A-Fa-f]{1,4}"
        - id: 10008
          pattern: "\\b\\d{6}\\b"
  )";

  v3::SensitiveDetectGlobal proto;
  TestUtility::loadFromYamlAndValidate(yaml, proto);
  auto decompressor_gzip_factory =
      std::make_unique<NiceMock<Compression::Decompressor::MockDecompressorFactory>>();
  auto decompressor_brotli_factory =
      std::make_unique<NiceMock<Compression::Decompressor::MockDecompressorFactory>>();
  decompressor_factory_gzip_ = decompressor_gzip_factory.get();
  decompressor_factory_brotli_ = decompressor_brotli_factory.get();
  auto config = std::make_shared<SensitiveDetectGlobalConfig>(
      proto, std::move(decompressor_gzip_factory), std::move(decompressor_brotli_factory));
  EXPECT_EQ(config->enable(), true);
  Buffer::OwnedImpl data_;
  std::string text = R"(
    "性别": 男 ,
    "日期": 2013-04-03 ,
    "营业执照号码": 90329528 ,
    "省份": 湖南省 ,
    "IPV6": "3e79:a603:d35b:f3c1:9ccb:19db:36aa:a9c2" ,
    "邮政编码": "934217"
  )";
  data_.add(text);
  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  SensitiveDetect filter(config, context);
  filter.thread_index_ = 0; //mock thread index

  Http::TestResponseHeaderMapImpl headers{
      {":authority", "localhost:8888"},
      {":path", "/"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "curl/7.68.0"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"end-user", "envoy"},
      {"content-length", "441"},
      {"content-type", "text/json; charset=utf-8"},
  };
  filter.encodeHeaders(headers, false);
  filter.encodeData(data_, true);
  filter.onStreamComplete();
  EXPECT_EQ(filter.getLog().sensitives_size(), 6);
}

// 测试streamscan, 匹配字符串分别在两个不同stream的情况
TEST_F(SensitiveDetectTest, TestStreamScan) {
  std::string text_front = R"({
  "IPv4": "134.231.87.27",
  "IPv6": "3e79:a603:d35b:f3c1:9ccb:19db:36aa:a9c2",
  "MAC 地址": "41:2a:1)";
  std::string text_behind = R"(2:2e:98:d4",
  "个人普通护照号码": "G1279559",
  "公务护照号码": "S63588095",
  "固定电话": "0755-95723422",
  "域名": "guo.net",
  "外交护照号码": "D33849343",
  "姓名": "罗建平",
  "性别": "男",
  "手机号号码": "14585855702",
  "日期": "2013-04-03",
  "民族": "汉族",
  "港澳通行证": "C97423997",
  "省份": "湖南省",
  "组织机构代码": "M748443369X8474994",
  "营业执照号码": 90329528,
  "身份证号码": "213000195311062642",
  "车牌号": "吉TMC9X6C",
  "车辆识别码": "4GDBVR7G156R70W3N",
  "邮政编码": "934217",
  "邮箱": "zqiu@example.com",
  "银行卡号": "6216604003121175"
})";
  // 不是压缩编码的情况
  auto config = makeConfig();
  EXPECT_EQ(config->enable(), true);
  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  SensitiveDetect filter(config, context);
  filter.thread_index_ = 0; //mock thread index
  Http::TestResponseHeaderMapImpl headers{
      {":authority", "localhost:8888"},
      {":path", "/"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "curl/7.68.0"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"end-user", "envoy"},
      {"content-length", "441"},
      {"content-type", "text/json; charset=utf-8"},
  };
  filter.encodeHeaders(headers, false);
  Buffer::OwnedImpl data_;
  data_.add(text_front);
  filter.encodeData(data_, false);
  data_.drain(data_.length());
  data_.add(text_behind);
  filter.encodeData(data_, true);
  filter.onStreamComplete();
  EXPECT_EQ(filter.getLog().sensitives_size(), 14);
}

// 测试日志
TEST_F(SensitiveDetectTest, TestOnStreamComplete) {
  // 不是压缩编码的情况下
  auto config = makeConfig();
  EXPECT_EQ(config->enable(), true);
  Buffer::OwnedImpl data_;
  data_.add(text);
  NiceMock<Server::Configuration::MockServerFactoryContext> context;
  SensitiveDetect filter(config, context);
  filter.thread_index_ = 0; //mock thread index
  Http::TestResponseHeaderMapImpl headers{
      {":authority", "localhost:8888"},
      {":path", "/"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "curl/7.68.0"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"end-user", "envoy"},
      {"content-length", "441"},
      {"content-type", "text/json; charset=utf-8"},
  };
  filter.encodeHeaders(headers, false);
  filter.encodeData(data_, false);
  filter.onStreamComplete();
  EXPECT_EQ(filter.getLog().sensitives_size(), 14);

  // 是压缩编码的情况下
  Http::TestResponseHeaderMapImpl headers_br{
      {":authority", "localhost:8888"},
      {":path", "/"},
      {":method", "GET"},
      {":scheme", "http"},
      {"user-agent", "curl/7.68.0"},
      {"accept", "*/*"},
      {"x-forwarded-proto", "http"},
      {"end-user", "envoy"},
      {"content-length", "441"},
      {"content-type", "text/json; charset=utf-8"},
      {"Content-Encoding", "br"},
  };
  SensitiveDetect filter_br(config, context);
  filter_br.thread_index_ = 0; //mock thread index
  filter_br.encodeHeaders(headers_br, false);
  filter_br.encodeData(data_, false);
  filter_br.onStreamComplete();
  EXPECT_EQ(filter_br.getLog().sensitives_size(), 14);
}

} // namespace SensitiveDetect
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
