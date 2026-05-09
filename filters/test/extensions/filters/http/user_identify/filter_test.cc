#include "test/mocks/server/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filters/source/extensions/filters/http/user_identify/config.h"
#include "filters/source/extensions/filters/http/user_identify/user_identify.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace UserIdentify {

namespace v3 = envoy::extensions::filters::http::user_identify::v3;

class UserIdentifyTest : public testing::Test {
public:
  UserIdentifyTest() = default;
  void setup(const std::string& yaml) {
    v3::UserIdentifyGlobal global_config;

    if (!yaml.empty()) {
      TestUtility::loadFromYamlAndValidate(yaml, global_config);
    }

    client_manage_ = std::make_shared<NiceMock<Grpc::MockAsyncClientManager>>();
    async_client_ = std::make_shared<NiceMock<Grpc::MockAsyncClient>>();

    ON_CALL(context_, clusterManager()).WillByDefault(ReturnRef(cluster_));
    ON_CALL(cluster_, grpcAsyncClientManager()).WillByDefault(ReturnRef(*client_manage_));
    ON_CALL(*client_manage_, getOrCreateRawAsyncClient(_, _, _, _))
        .WillByDefault(Return(async_client_));

    const std::chrono::milliseconds timeout =
        std::chrono::milliseconds(PROTOBUF_GET_MS_OR_DEFAULT(global_config.grpc(), timeout, 20));
    auto client_ = std::make_unique<Filters::Common::CentralDatabase::Database>(
        context_, global_config.grpc(), timeout, FILTER_NAME);
    global_config_ = std::make_shared<UserIdentifyFilterGlobalConfig>(
        global_config, context_, "sign_in\r\nlogin", "user\r\nusername", "token\r\nsession",
        "user_id\r\nguest_id");
    filter_ =
        std::make_shared<UserIdentifyFilter>(global_config_, std::move(client_), server_context_);

    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    filter_->setEncoderFilterCallbacks(encoder_callbacks_);
    ON_CALL(decoder_callbacks_, decoderBufferLimit()).WillByDefault(Return(200));
    ON_CALL(encoder_callbacks_, encoderBufferLimit()).WillByDefault(Return(200));
    cluster_info_ = std::make_shared<NiceMock<Envoy::Upstream::MockClusterInfo>>();
    ON_CALL(decoder_callbacks_, streamInfo()).WillByDefault(ReturnRef(stream_info_));
    ON_CALL(stream_info_, upstreamClusterInfo()).WillByDefault(Return(cluster_info_));
    ON_CALL(encoder_callbacks_, streamInfo()).WillByDefault(ReturnRef(stream_info_));
    ON_CALL(encoder_callbacks_, activeSpan()).WillByDefault(ReturnRef(span_));
  }

public:
  std::shared_ptr<UserIdentifyFilter> filter_;
  std::unique_ptr<Filters::Common::CentralDatabase::Database> client_;
  std::shared_ptr<UserIdentifyFilterGlobalConfig> global_config_;
  std::shared_ptr<NiceMock<Grpc::MockAsyncClient>> async_client_;
  std::shared_ptr<Http::MockAsyncClientRequest> request_;
  std::shared_ptr<NiceMock<Envoy::Upstream::MockClusterInfo>> cluster_info_;
  std::shared_ptr<Upstream::MockClusterManager> cluster_manager_;
  std::shared_ptr<NiceMock<Grpc::MockAsyncClientManager>> client_manage_;
  NiceMock<Tracing::MockSpan> span_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_callbacks_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info_;
  NiceMock<Server::Configuration::MockServerFactoryContext> server_context_;
  NiceMock<Server::Configuration::MockFactoryContext> context_;
  NiceMock<Upstream::MockClusterManager> cluster_;
};

// 验证不存在网关级配置则直接放行
TEST_F(UserIdentifyTest, NoGolbalConfig) {
  setup("");
  filter_ = std::make_shared<UserIdentifyFilter>(nullptr, std::move(client_), server_context_);
  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
}

// 验证请求资源为png、css、html直接放行
TEST_F(UserIdentifyTest, ResourceIsNotHandler) {
  const std::string yaml = R"(
    auto_mode: true
    grpc:
      envoy_grpc:
        cluster_name: grpc_cluster
      timeout: 20s
    )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("hello.css");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  auto response_header = Http::TestResponseHeaderMapImpl();

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_header, false));
  Buffer::OwnedImpl buffer("nothing here");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(buffer, true));
}

// 验证在响应头中没有找到Set-Cookie直接放行
TEST_F(UserIdentifyTest, ResponseHeadersHasNotToken) {
  const std::string yaml = R"(
    auto_mode: true
    grpc:
      envoy_grpc:
        cluster_name: grpc_cluster
    )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("hello/hello");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  auto response_header = Http::TestResponseHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_header, true));
}

// 验证自定义模式下，在请求行中匹配用户名
TEST_F(UserIdentifyTest, CustomizeMatchUserInHeader) {
  const std::string yaml = R"(
    customize:
      customization:
        cluster_name: ["fake_cluster"]
        user_name: ["sign_in"]
        enabled: true
        url: ["git.ouryun.cn/users/sign_in"]
    )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("users/sign_in?sign_in=hello");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl buffer("nothing here");
  Buffer::OwnedImpl decoding_buffer;
  EXPECT_CALL(decoder_callbacks_, addDecodedData(_, false))
      .WillRepeatedly(Invoke([&](Buffer::Instance& data, bool) { decoding_buffer.move(data); }));
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillRepeatedly(Return(&decoding_buffer));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(buffer, true));
  EXPECT_CALL(encoder_callbacks_, encodingBuffer()).WillRepeatedly(Return(&buffer));
  EXPECT_EQ(filter_->encodeData(buffer, true), Http::FilterDataStatus::Continue);
  EXPECT_EQ(filter_->user_name_, "hello");
}

// 验证自定义模式下，在请求体中匹配用户名
TEST_F(UserIdentifyTest, CustomizeMatchUserInBody) {
  const std::string yaml = R"(
    customize:
      customization:
        cluster_name: ["fake_cluster"]
        user_name: ["sign_in"]
        enabled: true
        url: ["git.ouryun.cn/users/sign_in"]
    )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("users/sign_in");
  headers.setContentType("application/json");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl buffer("\"sign_in\":\"hello\"");
  Buffer::OwnedImpl decoding_buffer;
  EXPECT_CALL(decoder_callbacks_, addDecodedData(_, false))
      .WillRepeatedly(Invoke([&](Buffer::Instance& data, bool) { decoding_buffer.move(data); }));
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillRepeatedly(Return(&decoding_buffer));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(buffer, true));
  EXPECT_EQ(filter_->user_name_, "hello");
}

// 验证自定义模式下URL为空
TEST_F(UserIdentifyTest, CustomizeAndUrlsIsEmpty) {
  const std::string yaml = R"(
    customize:
      customization:
        cluster_name: ["fake_cluster"]
        user_name: ["user"]
        enabled: true
        url: []
    )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("git.ouryun.cn/users/sign_in?user=hello");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl buffer("nothing here");
  EXPECT_EQ(filter_->decodeData(buffer, true), Http::FilterDataStatus::Continue);
  EXPECT_EQ(filter_->user_name_, "hello");
}

// 验证自定义模式下，从请求体中获取用户名，在响应体中获取token
TEST_F(UserIdentifyTest, CustomizeMatchUserFromBody) {
  const std::string yaml = R"(
    customize:
      customization:
        cluster_name: fake_cluster
        user_name: ["user"]
        enabled: true
        url: git.ouryun.cn/users/users
    )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("git.ouryun.cn/users");
  headers.setContentType("application/json");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl buffer("\"user\":\"hello\"");
  Buffer::OwnedImpl decoding_buffer;
  EXPECT_CALL(decoder_callbacks_, addDecodedData(_, false))
      .WillRepeatedly(Invoke([&](Buffer::Instance& data, bool) { decoding_buffer.move(data); }));
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillRepeatedly(Return(&decoding_buffer));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(buffer, true));
  EXPECT_EQ(filter_->user_name_, "hello");

  Buffer::OwnedImpl encoding_buffer("\"token\":\"23456744489\"");
  EXPECT_CALL(*async_client_, sendRaw(_, _, _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        Impl::UserInfo info("abcd", 200);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>(&info, sizeof(info));
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return async_client_->async_request_.get();
      }));
  EXPECT_EQ(filter_->encodeData(encoding_buffer, true), Http::FilterDataStatus::StopIterationAndWatermark);
  EXPECT_EQ(filter_->token_, "23456744489");
  filter_->onStreamComplete();
}

// 验证自动模式下，从请求体中获取用户名
TEST_F(UserIdentifyTest, AutoModeMatchUserFromBody) {
  const std::string yaml = R"(
    auto_mode: true
    grpc:
      envoy_grpc:
        cluster_name: grpc_cluster
    )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("git.ouryun.cn/sign_in");
  headers.setContentType("application/json");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl buffer("\"user\":\"hello\"");
  Buffer::OwnedImpl decoding_buffer;
  EXPECT_CALL(decoder_callbacks_, addDecodedData(_, false))
      .WillRepeatedly(Invoke([&](Buffer::Instance& data, bool) { decoding_buffer.move(data); }));
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillRepeatedly(Return(&decoding_buffer));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(buffer, true));
  EXPECT_EQ(filter_->user_name_, "hello");
}

// 验证自动模式下，从请求体中获取用户名且数据格式为xml
TEST_F(UserIdentifyTest, AutoModeMatchUserFromBodyAndXml) {
  const std::string yaml = R"(
    auto_mode: true
    grpc:
      envoy_grpc:
        cluster_name: grpc_cluster
    )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("git.ouryun.cn/sign_in");
  headers.setContentType("application/xml");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl buffer("<username>xd</username>");
  Buffer::OwnedImpl decoding_buffer;
  EXPECT_CALL(decoder_callbacks_, addDecodedData(_, false))
      .WillRepeatedly(Invoke([&](Buffer::Instance& data, bool) { decoding_buffer.move(data); }));
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillRepeatedly(Return(&decoding_buffer));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(buffer, true));
  EXPECT_EQ(filter_->user_name_, "xd");
}

// 验证自定义模式下，从请求体中获取用户名且数据格式为xml
TEST_F(UserIdentifyTest, CustomizeModeMatchUserFromBodyAndXml) {
  const std::string yaml = R"(
    customize:
      customization:
        cluster_name: fake_cluster
        user_name: ["username"]
        enabled: true
        url: git.ouryun.cn/sign_in
    )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("git.ouryun.cn/sign_in");
  headers.setContentType("application/xml");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl buffer("<username>xd</username>");
  Buffer::OwnedImpl decoding_buffer;
  std::cout << filter_->upstream_ << std::endl;
  EXPECT_CALL(decoder_callbacks_, addDecodedData(_, false))
      .WillRepeatedly(Invoke([&](Buffer::Instance& data, bool) { decoding_buffer.move(data); }));
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillRepeatedly(Return(&decoding_buffer));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(buffer, true));
  EXPECT_EQ(filter_->user_name_, "xd");
}

// 验证自定义模式下，没有匹配到业务
TEST_F(UserIdentifyTest, CustomizeMatchUserNoCluster) {
  const std::string yaml = R"(
    customize:
      customization:
        cluster_name: local_cluster
        user_name: ["user"]
        enabled: true
        url: git.ouryun.cn/users
    )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("git.ouryun.cn/users/sign_in");
  headers.setContentType("application/json");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl buffer("\"user\":\"hello\"");
  Buffer::OwnedImpl decoding_buffer;
  EXPECT_CALL(decoder_callbacks_, addDecodedData(_, false))
      .WillRepeatedly(Invoke([&](Buffer::Instance& data, bool) { decoding_buffer.move(data); }));
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillRepeatedly(Return(&decoding_buffer));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(buffer, true));
  EXPECT_EQ(filter_->user_name_, "");
}

// 验证自定义模式下，请求体数据没有接受完，暂存数据
TEST_F(UserIdentifyTest, CustomizeAndBuffer) {
  const std::string yaml = R"(
    customize:
      customization:
        cluster_name: local_cluster
        user_name: ["user"]
        enabled: true
        url: git.ouryun.cn/users
    )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("git.ouryun.cn/users/sign_in");
  headers.setContentType("application/json");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl buffer("\"user\":\"hello\"");
  Buffer::OwnedImpl decoding_buffer;
  EXPECT_CALL(decoder_callbacks_, addDecodedData(_, false))
      .WillRepeatedly(Invoke([&](Buffer::Instance& data, bool) { decoding_buffer.move(data); }));
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillRepeatedly(Return(&decoding_buffer));
  EXPECT_EQ(Http::FilterDataStatus::StopIterationAndBuffer, filter_->decodeData(buffer, false));
}

// 验证自动模式下，是登录请求，成功获取到token
TEST_F(UserIdentifyTest, AutoModeAndIsLogin) {
  const std::string yaml = R"(
     auto_mode: true
     grpc:
       envoy_grpc:
         cluster_name: grpc_cluster
       timeout: 20s
     )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("git.ouryun.cn/users/sign_in?user=hello");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl buffer("\"token\":\"345\"");
  Buffer::OwnedImpl encoding_buffer;

  auto response_header = Http::TestResponseHeaderMapImpl();
  response_header.addCopy(Http::LowerCaseString("Set-Cookie"), "token=3323232323233");
  const auto& map = headers.get(Http::LowerCaseString("Set-Cookie"));

  EXPECT_CALL(*async_client_, sendRaw(_, _, _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        Impl::UserInfo info("abcd", 200);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>(&info, sizeof(info));
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return async_client_->async_request_.get();
      }));
  EXPECT_EQ(Http::FilterHeadersStatus::StopAllIterationAndWatermark, filter_->encodeHeaders(response_header, true));
  EXPECT_CALL(encoder_callbacks_, encodingBuffer()).WillRepeatedly(Return(&buffer));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(buffer, true));
  EXPECT_EQ(filter_->user_name_, "hello");
  EXPECT_EQ(filter_->token_, "3323232323233");
}

// 验证自动模式下，访问请求，在请求头获取到了token
TEST_F(UserIdentifyTest, AutoModeAndIsNotLogin) {
  const std::string yaml = R"(
     auto_mode: true
     grpc:
       envoy_grpc:
         cluster_name: grpc_cluster
       timeout: 20s
     )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("git.ouryun.cn/users/hello");
  headers.setByReferenceKey("cookie", "guest_id=jdjdjdjjdjjd");
  EXPECT_EQ(Http::FilterHeadersStatus::StopAllIterationAndWatermark, filter_->decodeHeaders(headers, false));
  EXPECT_FALSE(filter_->is_login_);
  EXPECT_EQ(filter_->token_, "jdjdjdjjdjjd");
  auto response_header = Http::TestResponseHeaderMapImpl();
  response_header.addCopy(Http::LowerCaseString("Set-Cookie"), "token=333");
  // const auto& map = headers.get(Http::LowerCaseString("Set-Cookie"));
  EXPECT_CALL(*async_client_, sendRaw(_, _, _, _, _, _))
      .WillRepeatedly(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                                 Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                                 const Http::AsyncClient::RequestOptions&) {
        const std::string data =
            "\x08\x01\x10\xd2\x04\x18\x0c\x65\x78\x61\x6d\x70\x6c\x65\x20\x76\x61\x6c\x75\x65";
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return async_client_->async_request_.get();
      }));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_header, true));
  // filter_->complete();
}

// 验证自动模式下，访问请求没有获得响应token
TEST_F(UserIdentifyTest, AutoModeAndIsNotLoginAndNoResponseToken) {
  const std::string yaml = R"(
     auto_mode: true
     grpc:
       envoy_grpc:
         cluster_name: grpc_cluster
       timeout: 20s
     )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("git.ouryun.cn/users/hello");
  headers.setByReferenceKey("cookie", "token=jdjdjdjjdjjd");
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  auto response_header = Http::TestResponseHeaderMapImpl();
  EXPECT_CALL(*async_client_, sendRaw(_, _, _, _, _, _))
      .WillRepeatedly(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                                 Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                                 const Http::AsyncClient::RequestOptions&) {
        const std::string data =
            "\x08\x01\x10\xd2\x04\x18\x0c\x65\x78\x61\x6d\x70\x6c\x65\x20\x76\x61\x6c\x75\x65";
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return async_client_->async_request_.get();
      }));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_header, true));
  EXPECT_FALSE(filter_->is_login_);
  EXPECT_EQ(filter_->token_, "");
}

// 验证自动模式下，访问请求获取到访客token
TEST_F(UserIdentifyTest, AutoModeAndIsNotLoginGetGuestToken) {
  const std::string yaml = R"(
     auto_mode: true
     grpc:
       envoy_grpc:
         cluster_name: grpc_cluster
     )";
  setup(yaml);
  auto headers = Http::TestRequestHeaderMapImpl();
  headers.setPath("git.ouryun.cn/users/hello");
  headers.setByReferenceKey("cookie", "guest_id=hello");
  EXPECT_EQ(Http::FilterHeadersStatus::StopAllIterationAndWatermark, filter_->decodeHeaders(headers, true));
  EXPECT_FALSE(filter_->is_login_);
  EXPECT_EQ(filter_->token_, "hello");

  Buffer::OwnedImpl buffer("nothing here");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(buffer, true));

  auto response_header = Http::TestResponseHeaderMapImpl();
  response_header.addCopy(Http::LowerCaseString("Set-Cookie"), "token=33377666666;");
  EXPECT_CALL(*async_client_, sendRaw(_, _, _, _, _, _))
      .WillRepeatedly(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                                 Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                                 const Http::AsyncClient::RequestOptions&) {
        const std::string data =
            "\x08\x01\x10\xd2\x04\x18\x0c\x65\x78\x61\x6d\x70\x6c\x65\x20\x76\x61\x6c\x75\x65";
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        async_client_->async_request_ = std::make_unique<NiceMock<Grpc::MockAsyncRequest>>();
        return async_client_->async_request_.get();
      }));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_header, true));
  // EXPECT_CALL(*async_client_->async_request_, cancel()).Times(1);
  filter_->onDestroy();
}

} // namespace UserIdentify
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy