#include "test/mocks/server/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filters/source/extensions/filters/http/weak_password_check/config.h"
#include "filters/source/extensions/filters/http/weak_password_check/weak_password_check.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WeakPasswordCheck {

namespace v3 = envoy::extensions::filters::http::weak_password_check::v3;

class WpdTest : public testing::Test {
public:
  WpdTest() = default;
  void setup(const std::string& yaml) {
    v3::WeakPasswordCheckGlobal global_config;

    if (!yaml.empty()) {
      TestUtility::loadFromYamlAndValidate(yaml, global_config);
    }

    ON_CALL(context_, clusterManager()).WillByDefault(ReturnRef(cluster_));
    global_config_ = std::make_shared<FilterGlobalConfig>(
        global_config, context_);
    filter_ = std::make_shared<Filter>(global_config_, server_context_);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    ON_CALL(decoder_callbacks_, decoderBufferLimit()).WillByDefault(Return(200));
    cluster_info_ = std::make_shared<NiceMock<Envoy::Upstream::MockClusterInfo>>();
    ON_CALL(decoder_callbacks_, streamInfo()).WillByDefault(ReturnRef(stream_info_));
    ON_CALL(stream_info_, upstreamClusterInfo()).WillByDefault(Return(cluster_info_));
  }

public:
  std::shared_ptr<Filter> filter_;
  std::shared_ptr<FilterGlobalConfig> global_config_;
  std::shared_ptr<NiceMock<Envoy::Upstream::MockClusterInfo>> cluster_info_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info_;
  NiceMock<Server::Configuration::MockServerFactoryContext> server_context_;
  NiceMock<Server::Configuration::MockFactoryContext> context_;
  NiceMock<Upstream::MockClusterManager> cluster_;
};

// 验证不存在网关级配置则直接放行
TEST_F(WpdTest, NoGolbalConfig) {
  setup("");
  filter_ = std::make_shared<Filter>(nullptr, server_context_);
  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  EXPECT_FALSE(filter_->deal_flag());
}

// 验证请求资源为png、css、html直接放行
TEST_F(WpdTest, ResourceIsNotHandler) {
  const std::string yaml = R"(
    auto_mode: true
    )";
  setup(yaml);

  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("/hello.css");

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  EXPECT_FALSE(filter_->deal_flag());
}


// 验证自动模式下，在请求行中检测密码
TEST_F(WpdTest, PasswordInRequestPath) {
  const std::string yaml = R"(
      auto_mode: true
      custom_password:
        rule_config:
          - part_rule: ["@", "dd", "123", "sr", "tiantian"]
          - part_rule: ["3", "david", "david", "zhongguo", "shanghai", "wo"]
        date_range: ["2014-01-11", "2024-02-21"]
        date_format: 7
        filter_instance_id: "weak-password.001"
    )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("users/sign_in?username=hello&password=123456");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));

  std::vector<std::tuple<bool, bool, std::string, std::string>> weak_passwords;
  filter_->getWeakPasswords(weak_passwords);
  EXPECT_EQ(weak_passwords.size(), 1);
  EXPECT_TRUE(std::get<0>(weak_passwords[0]));
  EXPECT_TRUE(std::get<1>(weak_passwords[0]));
  EXPECT_EQ(std::get<2>(weak_passwords[0]), "hello");
  EXPECT_EQ(std::get<3>(weak_passwords[0]), "123456");

}

// 验证自动模式下，在请求体中检测密码。弱密码在彩虹表中, 数据为json
TEST_F(WpdTest, PasswordInBody) {
  const std::string yaml = R"(
      auto_mode: true
      custom_password:
        rule_config:
          - part_rule: ["@", "dd", "123", "sr", "tiantian"]
          - part_rule: ["3", "david", "david", "zhongguo", "shanghai", "wo"]
        date_range: ["2014-01-11", "2024-02-21"]
        date_format: 7
        filter_instance_id: "weak-password.001"
    )";
  const std::string json_data = R"(
  {
	"lastname": "lusxh",
	"password": "3H8IDC72sanhe000",
	"rememberMe": true,
	"codes": "7576"
  }
  )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("users/sign_in");
  headers.setContentType("application/json");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl buffer(json_data);
  Buffer::OwnedImpl decoding_buffer;
  EXPECT_CALL(decoder_callbacks_, addDecodedData(_, false))
      .WillRepeatedly(Invoke([&](Buffer::Instance& data, bool) { decoding_buffer.move(data); }));
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillRepeatedly(Return(&decoding_buffer));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(buffer, true));

  std::vector<std::tuple<bool, bool, std::string, std::string>> weak_passwords;
  filter_->getWeakPasswords(weak_passwords);
  EXPECT_EQ(weak_passwords.size(), 1);
  EXPECT_TRUE(std::get<0>(weak_passwords[0]));
  EXPECT_TRUE(std::get<1>(weak_passwords[0]));
  EXPECT_EQ(std::get<2>(weak_passwords[0]), "lusxh");
  EXPECT_EQ(std::get<3>(weak_passwords[0]), "3H8IDC72sanhe000");

}

// 验证自动模式下，在请求体中检测密码。弱密码在彩虹表中, 数据为xml
TEST_F(WpdTest, PasswordInBodyXml) {
  const std::string yaml = R"(
      auto_mode: true
      custom_password:
        rule_config:
          - part_rule: ["@", "dd", "123", "sr", "tiantian"]
          - part_rule: ["3", "david", "david", "zhongguo", "shanghai", "wo"]
        date_range: ["2014-01-11", "2024-02-21"]
        date_format: 7
        filter_instance_id: "weak-password.001"
    )";
  const std::string json_data = R"(
  <sites>
    <site>
        <name>lusxh</name>
        <url>www.runoob.com</url>
        <password>3H8IDC72sanhe000</password>
        <rememberMe>true</rememberMe>
    </site>
  </sites>
  )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("users/sign_in");
  headers.setContentType("application/xml");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl buffer(json_data);
  Buffer::OwnedImpl decoding_buffer;
  EXPECT_CALL(decoder_callbacks_, addDecodedData(_, false))
      .WillRepeatedly(Invoke([&](Buffer::Instance& data, bool) { decoding_buffer.move(data); }));
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillRepeatedly(Return(&decoding_buffer));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(buffer, true));

  std::vector<std::tuple<bool, bool, std::string, std::string>> weak_passwords;
  filter_->getWeakPasswords(weak_passwords);
  EXPECT_EQ(weak_passwords.size(), 1);
  EXPECT_TRUE(std::get<0>(weak_passwords[0]));
  EXPECT_TRUE(std::get<1>(weak_passwords[0]));
  EXPECT_EQ(std::get<2>(weak_passwords[0]), "lusxh");
  EXPECT_EQ(std::get<3>(weak_passwords[0]), "3H8IDC72sanhe000");

}

// 验证自动模式下，在请求体中检测密码。弱密码在自定义彩虹表中
TEST_F(WpdTest, PasswordInBodyXmlCustom) {
  const std::string yaml = R"(
      auto_mode: true
      custom_password:
        rule_config:
          - part_rule: ["@", "dd", "123", "sr", "tiantian"]
          - part_rule: ["3", "david", "david", "zhongguo", "shanghai", "wo"]
        date_range: ["2014-01-11", "2024-02-21"]
        date_format: 7
        filter_instance_id: "weak-password.001"
    )";
  const std::string json_data = R"(
  <sites>
    <site>
        <name>lusxh</name>
        <url>www.runoob.com</url>
        <password>123shanghai</password>
        <rememberMe>true</rememberMe>
    </site>
  </sites>
  )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("users/sign_in");
  headers.setContentType("application/xml");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl buffer(json_data);
  Buffer::OwnedImpl decoding_buffer;
  EXPECT_CALL(decoder_callbacks_, addDecodedData(_, false))
      .WillRepeatedly(Invoke([&](Buffer::Instance& data, bool) { decoding_buffer.move(data); }));
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillRepeatedly(Return(&decoding_buffer));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(buffer, true));

  std::vector<std::tuple<bool, bool, std::string, std::string>> weak_passwords;
  filter_->getWeakPasswords(weak_passwords);
  EXPECT_EQ(weak_passwords.size(), 1);
  EXPECT_TRUE(std::get<0>(weak_passwords[0]));
  EXPECT_TRUE(std::get<1>(weak_passwords[0]));
  EXPECT_EQ(std::get<2>(weak_passwords[0]), "lusxh");
  EXPECT_EQ(std::get<3>(weak_passwords[0]), "123shanghai");

}

// 验证高级配置模式下，在请求体中检测密码。弱密码在内置彩虹表中
TEST_F(WpdTest, AdvancedPasswordInBodyXml) {
  const std::string yaml = R"(
      auto_mode: false
      advance_config:
        - user_name: ["email", "username","21212121","account"]
          password_name: ["pswwd","crypt"]
          cluster_name: ["fake_cluster", "cluster3", "service_oa"]
      custom_password:
        rule_config:
          - part_rule: ["@", "dd", "123", "sr", "tiantian"]
          - part_rule: ["3", "david", "david", "zhongguo", "shanghai", "wo"]
        date_range: ["2014-01-11", "2024-02-21"]
        date_format: 7
        filter_instance_id: "weak-password.001"
    )";
  const std::string json_data = R"(
  <sites>
    <site>
        <email>lusxh</email>
        <url>www.runoob.com</url>
        <pswwd>3H8IDC72sanhe000</pswwd>
        <rememberMe>true</rememberMe>
    </site>
  </sites>
  )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("users/sign_in");
  headers.setContentType("application/xml");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl buffer(json_data);
  Buffer::OwnedImpl decoding_buffer;
  EXPECT_CALL(decoder_callbacks_, addDecodedData(_, false))
      .WillRepeatedly(Invoke([&](Buffer::Instance& data, bool) { decoding_buffer.move(data); }));
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillRepeatedly(Return(&decoding_buffer));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(buffer, true));

  std::vector<std::tuple<bool, bool, std::string, std::string>> weak_passwords;
  filter_->getWeakPasswords(weak_passwords);
  EXPECT_EQ(weak_passwords.size(), 1);
  EXPECT_TRUE(std::get<0>(weak_passwords[0]));
  EXPECT_TRUE(std::get<1>(weak_passwords[0]));
  EXPECT_EQ(std::get<2>(weak_passwords[0]), "lusxh");
  EXPECT_EQ(std::get<3>(weak_passwords[0]), "3H8IDC72sanhe000");

}

// 验证高级配置模式下，在请求体中检测密码。弱密码在自定义彩虹表中
TEST_F(WpdTest, AdvancedPasswordInBodyXmlCustom) {
  const std::string yaml = R"(
      auto_mode: false
      advance_config:
        - user_name: ["email", "username","21212121","account"]
          password_name: ["pswwd","crypt"]
          cluster_name: ["fake_cluster", "cluster3", "service_oa"]
      custom_password:
        rule_config:
          - part_rule: ["@", "dd", "123", "sr", "tiantian"]
          - part_rule: ["3", "david", "david", "zhongguo", "shanghai", "wo"]
        date_range: ["2014-01-11", "2024-02-21"]
        date_format: 7
        filter_instance_id: "weak-password.001"
    )";
  const std::string json_data = R"(
  <sites>
    <site>
        <email>lusxh</email>
        <url>www.runoob.com</url>
        <pswwd>123shanghai</pswwd>
        <rememberMe>true</rememberMe>
    </site>
  </sites>
  )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("users/sign_in");
  headers.setContentType("application/xml");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl buffer(json_data);
  Buffer::OwnedImpl decoding_buffer;
  EXPECT_CALL(decoder_callbacks_, addDecodedData(_, false))
      .WillRepeatedly(Invoke([&](Buffer::Instance& data, bool) { decoding_buffer.move(data); }));
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillRepeatedly(Return(&decoding_buffer));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(buffer, true));

  std::vector<std::tuple<bool, bool, std::string, std::string>> weak_passwords;
  filter_->getWeakPasswords(weak_passwords);
  EXPECT_EQ(weak_passwords.size(), 1);
  EXPECT_TRUE(std::get<0>(weak_passwords[0]));
  EXPECT_TRUE(std::get<1>(weak_passwords[0]));
  EXPECT_EQ(std::get<2>(weak_passwords[0]), "lusxh");
  EXPECT_EQ(std::get<3>(weak_passwords[0]), "123shanghai");

}

} // namespace UserIdentify
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy