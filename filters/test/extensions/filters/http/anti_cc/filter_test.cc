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

#include "filters/source/extensions/filters/http/anti_cc/config.h"
#include "filters/source/extensions/filters/http/anti_cc/anti_cc.h"
#include "filters/source/extensions/filters/http/common/central_database/database.h"

using testing::NiceMock;
using testing::ReturnRef;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AntiCC {

namespace v3 = envoy::extensions::filters::http::anti_cc::v3;
namespace db_v3 = envoy::extensions::filters::http::common::central_database::v3;
namespace ratelimit_v3 = envoy::extensions::filters::http::common::ratelimit::v3;

static const std::string global_yaml = R"(
      mode: CUSTOM
      action:
        dryrun: false
        man_machine_verification: true
        check_quota:
          duration: 30
          max_count: 3
        rate_limit_quota:
          duration: 30
          max_count: 3
        block_time: 500
      external_grpc_server:
        envoy_grpc:
          cluster_name: grpc_service
        timeout: 1s
      verification_server:
        cluster_name: verify_service
        timeout: 1s
      polycube_server:
        cluster_name: polycube_service
        timeout: 1s
      ip_whitelist:
      - ip_list:
          list:
          - address_prefix: "10.200.200.220"
        enable: true
  )";

class AntiCCTest : public testing::Test {
public:
  AntiCCTest() = default;

  void setAddressToReturn(const std::string& address) {
    downstream_connection_info_provider_->setRemoteAddress(Network::Utility::resolveUrl(address));
  }

  void setup(const std::string& yaml) {
    v3::AntiCCGlobal global_proto;
    if (!yaml.empty()) {
      TestUtility::loadFromYamlAndValidate(yaml, global_proto);
    }

    downstream_connection_info_provider_ =
        std::make_shared<Network::ConnectionInfoSetterImpl>(nullptr, nullptr);
    grpc_async_client_ = std::make_shared<NiceMock<Grpc::MockAsyncClient>>();

    ON_CALL(context_, clusterManager()).WillByDefault(ReturnRef(cluster_manager_));
    ON_CALL(cluster_manager_, grpcAsyncClientManager()).WillByDefault(ReturnRef(client_manage_));
    ON_CALL(client_manage_, getOrCreateRawAsyncClient(_, _, _, _))
        .WillByDefault(Return(grpc_async_client_));

    request_ = std::make_shared<NiceMock<Http::MockAsyncClientRequest>>(&http_async_client_);
    cluster_ = std::make_shared<NiceMock<Upstream::MockThreadLocalCluster>>();
    ON_CALL(server_context_, clusterManager()).WillByDefault(ReturnRef(server_cluster_manager_));
    ON_CALL(server_cluster_manager_, getThreadLocalCluster(_))
        .WillByDefault(Return(cluster_.get()));
    ON_CALL(*cluster_, httpAsyncClient()).WillByDefault(ReturnRef(http_async_client_));

    ON_CALL(stream_info_, downstreamAddressProvider())
        .WillByDefault(ReturnRef(*downstream_connection_info_provider_));
    ON_CALL(stream_info_, upstreamClusterInfo()).WillByDefault(Return(cluster_info_));
    ON_CALL(decoder_callbacks_, streamInfo()).WillByDefault(ReturnRef(stream_info_));
    ON_CALL(decoder_callbacks_, activeSpan()).WillByDefault(ReturnRef(span_));
    ON_CALL(decoder_callbacks_, dispatcher()).WillByDefault(ReturnRef(dispatcher_));

    if (global_proto.has_external_grpc_server()) {
      const std::chrono::milliseconds timeout = std::chrono::milliseconds(
          PROTOBUF_GET_MS_OR_DEFAULT(global_proto.external_grpc_server(), timeout, 20));
      ratelimit_client_ = Filters::Common::RatelimitClient::Impl::rateLimitClient(
          context_, global_proto.external_grpc_server(), timeout);
      blacklist_kv_db_.reset(new Filters::Common::CentralDatabase::Database(
          context_, global_proto.external_grpc_server(), timeout, FILTER_NAME));
      verify_status_client_.reset(new Filters::Common::CentralDatabase::Database(
          context_, global_proto.external_grpc_server(), timeout, FILTER_NAME));
    }

    global_config_ = std::make_shared<AntiCCFilterGlobalConfig>(global_proto, server_context_);

    filter_ = std::make_shared<AntiCCFilter>(context_, global_config_, std::move(blacklist_kv_db_),
                                             std::move(verify_status_client_),
                                             std::move(ratelimit_client_));
    filter_->setDecoderFilterCallbacks(decoder_callbacks_); // 将decoder_callbacks_应用到插件中
  }

public:
  NiceMock<Tracing::MockSpan> span_;
  NiceMock<Event::MockDispatcher> dispatcher_;
  NiceMock<Http::MockAsyncClient> http_async_client_;
  NiceMock<Grpc::MockAsyncClientManager> client_manage_;
  NiceMock<Upstream::MockClusterManager> cluster_manager_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info_;
  NiceMock<Server::Configuration::MockFactoryContext> context_;
  NiceMock<Upstream::MockClusterManager> server_cluster_manager_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Server::Configuration::MockServerFactoryContext> server_context_;
  std::shared_ptr<NiceMock<Http::MockAsyncClientRequest>> request_;
  std::shared_ptr<NiceMock<Grpc::MockAsyncClient>> grpc_async_client_;
  std::shared_ptr<NiceMock<Upstream::MockThreadLocalCluster>> cluster_;
  std::shared_ptr<NiceMock<Envoy::Upstream::MockClusterInfo>> cluster_info_;
  std::shared_ptr<Network::ConnectionInfoSetterImpl> downstream_connection_info_provider_;

  AntiCCFilterFactory factory_;
  std::shared_ptr<AntiCCFilter> filter_;
  std::shared_ptr<AntiCCFilterGlobalConfig> global_config_;
  std::unique_ptr<Filters::Common::RatelimitClient::Client> ratelimit_client_;
  std::unique_ptr<Filters::Common::CentralDatabase::Database> blacklist_kv_db_;
  std::unique_ptr<Filters::Common::CentralDatabase::Database> verify_status_client_;
  static std::string filter_name_;
};
std::string AntiCCTest::filter_name_(FILTER_NAME);

// 验证匹配白名单
TEST_F(AntiCCTest, MatchWhiteList) {
  // 验证CidrRang类型白名单，掩码：32
  {
    std::string yaml_local = R"(
    external_grpc_server:
      envoy_grpc:
        cluster_name: grpc_cluster
      timeout: 2s
    ip_whitelist:
    - ip_list:
        list:
        - address_prefix: "10.200.200.220"
          prefix_len: 32
      enable: true
    )";
    setup(yaml_local);
    setAddressToReturn("tcp://10.200.200.220:20000");

    auto headers = Http::TestRequestHeaderMapImpl();
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  }

  // 验证CidrRang类型白名单,掩码：24
  {
    std::string yaml_local = R"(
    external_grpc_server:
      envoy_grpc:
        cluster_name: grpc_cluster
      timeout: 2s
    ip_whitelist:
    - ip_list:
        list:
        - address_prefix: "10.200.200.1"
          prefix_len: 24
      enable: true
    )";
    setup(yaml_local);
    setAddressToReturn("tcp://10.200.200.220:20000");
    auto headers = Http::TestRequestHeaderMapImpl();
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  }

  // 验证IpRange类型白名单
  {
    std::string yaml_local = R"(
    external_grpc_server:
      envoy_grpc:
        cluster_name: grpc_cluster
      timeout: 2s
    ip_whitelist:
    - ip_range_list:
      - start_ip: "10.200.200.1"
        end_ip: "10.200.200.255"
      enable: true
    )";
    setup(yaml_local);
    setAddressToReturn("tcp://10.200.200.220:20000");
    auto headers = Http::TestRequestHeaderMapImpl();
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  }

  // 验证白名单不启用
  {
    std::string yaml_local = R"(
    external_grpc_server:
      envoy_grpc:
        cluster_name: grpc_cluster
      timeout: 2s
    ip_whitelist:
    - ip_list:
        list:
        - address_prefix: "10.200.200.220"
          prefix_len: 32
      enable: false
    )";
    setup(yaml_local);
    setAddressToReturn("tcp://10.200.200.220:20000");

    auto headers = Http::TestRequestHeaderMapImpl();
    EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
  }

  // 验证未命中白名单
  {
    std::string yaml_local = R"(
    external_grpc_server:
      envoy_grpc:
        cluster_name: grpc_cluster
      timeout: 2s
    ip_whitelist:
    - ip_list:
        list:
        - address_prefix: "10.200.200.220"
          prefix_len: 32
      enable: true
    external_grpc_server:
      envoy_grpc:
        cluster_name: grpc_service
      timeout: 1s
    )";
    setup(yaml_local);
    setAddressToReturn("tcp://10.200.200.121:20000");

    auto headers = Http::TestRequestHeaderMapImpl();
    EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
  }
}

// 验证命中远程黑名单
TEST_F(AntiCCTest, IsMatchRemoteBlackList) {
  std::string global_yaml = R"(
  mode: CUSTOM
  action:
    rate_limit_quota:
      duration: 30
      max_count: 3
    block_time: 500
  external_grpc_server:
    envoy_grpc:
      cluster_name: grpc_service
    timeout: 1s
  verification_server:
    cluster_name: verify_service
    timeout: 1s
  polycube_server:
    cluster_name: polycube_service
    timeout: 1s
  )";
  setup(global_yaml);
  setAddressToReturn("tcp://10.200.200.121:20000");

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::TooManyRequests, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)> modify_headers,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::TooManyRequests, code);
        EXPECT_EQ("", body);
        EXPECT_EQ(grpc_status, absl::nullopt);
        EXPECT_EQ(details, "request_rate_limited");
      }));
  EXPECT_CALL(http_async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        Http::ResponseMessagePtr response(new Http::ResponseMessageImpl(
            Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status",
            "201"}}}));
        response->headers().setContentType(Http::Headers::get().ContentTypeValues.Json);
        response->body().add("{\"result\":true}");
        callbacks.onSuccess(*request_, std::move(response));
        return request_.get();
      }));
  EXPECT_CALL(*grpc_async_client_, sendRaw(_, _, _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        db_v3::GetResponse response_message;
        response_message.set_result(1);
        response_message.set_user_data(255);
        std::string value("11");
        response_message.set_value(value.c_str(), value.size() + 1);
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }));

  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
}

// 验证未限速,放行
TEST_F(AntiCCTest, NotLimit) {
  std::string global_yaml = R"(
  mode: CUSTOM
  action:
    rate_limit_quota:
      duration: 30
      max_count: 3
    block_time: 500
  external_grpc_server:
    envoy_grpc:
      cluster_name: grpc_service
    timeout: 1s
  verification_server:
    cluster_name: verify_service
    timeout: 1s
  polycube_server:
    cluster_name: polycube_service
    timeout: 1s
  )";
  setup(global_yaml);
  setAddressToReturn("tcp://10.200.200.121:20000");
  // 请求成功，放行

  EXPECT_CALL(decoder_callbacks_, continueDecoding());
  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "ShouldRateLimit", _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        std::cout << "limit" << std::endl;
        ratelimit_v3::RateLimitResponse response_message;
        response_message.set_status(ratelimit_v3::RateLimitResponse_Status_OK);
        response_message.set_duration(30);
        response_message.set_remaining(0);
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }));

  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "Get", _, _, _, _))
      .WillRepeatedly(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                                 Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                                 const Http::AsyncClient::RequestOptions&) {
        db_v3::GetResponse response_message;
        response_message.set_result(0);
        response_message.set_user_data(255);
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }));

  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
}

// 验证限速服务超时返回500
TEST_F(AntiCCTest, LimitError) {
  std::string global_yaml = R"(
  mode: CUSTOM
  action:
    rate_limit_quota:
      duration: 30
      max_count: 3
    block_time: 500
  external_grpc_server:
    envoy_grpc:
      cluster_name: grpc_service
    timeout: 1s
  verification_server:
    cluster_name: verify_service
    timeout: 1s
  polycube_server:
    cluster_name: polycube_service
    timeout: 1s
  )";
  setup(global_yaml);
  setAddressToReturn("tcp://10.200.200.121:20000");
  // 请求成功，放行

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::InternalServerError, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)>,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::InternalServerError, code);
        EXPECT_EQ("", body);
        EXPECT_EQ(grpc_status, absl::nullopt);
        EXPECT_EQ(details, "rate_limiter_error");
      }));
  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "ShouldRateLimit", _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        callbacks.onFailure(Grpc::Status::Unavailable, "", span_);
        return grpc_async_client_->async_request_.get();
      }));

  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "Get", _, _, _, _))
      .WillRepeatedly(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                                 Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                                 const Http::AsyncClient::RequestOptions&) {
        db_v3::GetResponse response_message;
        response_message.set_result(0);
        response_message.set_user_data(255);
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }));

  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
}

// 验证限速请求拒绝
TEST_F(AntiCCTest, ShouldLimit) {
  std::string global_yaml = R"(
  mode: CUSTOM
  action:
    rate_limit_quota:
      duration: 30
      max_count: 3
    block_time: 500
  external_grpc_server:
    envoy_grpc:
      cluster_name: grpc_service
    timeout: 1s
  verification_server:
    cluster_name: verify_service
    timeout: 1s
  polycube_server:
    cluster_name: polycube_service
    timeout: 1s
  )";
  setup(global_yaml);
  setAddressToReturn("tcp://10.200.200.121:20000");

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::TooManyRequests, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)> modify_headers,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::TooManyRequests, code);
        EXPECT_EQ("", body);
        EXPECT_EQ(grpc_status, absl::nullopt);
        EXPECT_EQ(details, "request_rate_limited");
      }));
  EXPECT_CALL(http_async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        Http::ResponseMessagePtr response(new Http::ResponseMessageImpl(
            Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status",
            "201"}}}));
        response->headers().setContentType(Http::Headers::get().ContentTypeValues.Json);
        response->body().add("{\"result\":true}");
        callbacks.onSuccess(*request_, std::move(response));
        return request_.get();
      }));
  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "Insert", _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        db_v3::InsertResponse response_message;
        response_message.set_result(1);
        response_message.set_user_data(255);
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }));
  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "ShouldRateLimit", _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        ratelimit_v3::RateLimitResponse response_message;
        response_message.set_status(ratelimit_v3::RateLimitResponse_Status_OVER_LIMIT);
        response_message.set_duration(30);
        response_message.set_remaining(0);
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }));
  Grpc::RawAsyncRequestCallbacks* cb{nullptr};
  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "Get", _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        db_v3::GetResponse response_message;
        response_message.set_result(0);
        response_message.set_user_data(255);
        response_message.set_value("foo");
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        cb = &callbacks;
        return grpc_async_client_->async_request_.get();
      }));

  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));

  db_v3::GetResponse response_message;
  response_message.set_result(0);
  response_message.set_user_data(255);
  response_message.set_value("foo");
  std::string serialized_data;
  response_message.SerializeToString(&serialized_data);
  std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
  buffer->add(serialized_data);
  cb->onSuccessRaw(std::move(buffer), span_);
}

// 验证限速且空转
TEST_F(AntiCCTest, Dryrun) {
  std::string global_yaml = R"(
  mode: CUSTOM
  action:
    dryrun: true
    rate_limit_quota:
      duration: 30
      max_count: 3
    block_time: 500
  external_grpc_server:
    envoy_grpc:
      cluster_name: grpc_service
    timeout: 1s
  verification_server:
    cluster_name: verify_service
    timeout: 1s
  polycube_server:
    cluster_name: polycube_service
    timeout: 1s
  )";
  setup(global_yaml);
  setAddressToReturn("tcp://10.200.200.121:20000");

  EXPECT_CALL(decoder_callbacks_, continueDecoding());
  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "ShouldRateLimit", _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        ratelimit_v3::RateLimitResponse response_message;
        response_message.set_status(ratelimit_v3::RateLimitResponse_Status_OVER_LIMIT);
        response_message.set_duration(30);
        response_message.set_remaining(0);
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }));

  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "Get", _, _, _, _))
      .WillRepeatedly(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                                 Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                                 const Http::AsyncClient::RequestOptions&) {
        db_v3::GetResponse response_message;
        response_message.set_result(0);
        response_message.set_user_data(255);
        response_message.set_value("foo");
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }));

  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
}

// 验证限速且人机验证
TEST_F(AntiCCTest, ManMachineVerification) {
  std::string global_yaml = R"(
  mode: CUSTOM
  action:
    man_machine_verification: true
    check_quota:
      duration: 30
      max_count: 3
    rate_limit_quota:
      duration: 30
      max_count: 3
    block_time: 500
  external_grpc_server:
    envoy_grpc:
      cluster_name: grpc_service
    timeout: 1s
  verification_server:
    cluster_name: verify_service
    timeout: 1s
  polycube_server:
    cluster_name: polycube_service
    timeout: 1s
  )";
  setup(global_yaml);
  setAddressToReturn("tcp://10.200.200.121:20000");

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::OK, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)> modify_headers,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::OK, code);
        EXPECT_EQ("fake_html", body);

        Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
        modify_headers(response_headers);
        EXPECT_TRUE(response_headers.has(Http::LowerCaseString("content-type")));

        EXPECT_EQ(grpc_status, absl::nullopt);
        EXPECT_EQ(details, "");
      }));
  EXPECT_CALL(http_async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        Http::ResponseMessagePtr response(new Http::ResponseMessageImpl(
            Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status",
            "200"}}}));
        response->headers().setContentType(Http::Headers::get().ContentTypeValues.Json);
        std::string body = R"(
        {
          "code": 200,
          "data":
          {
            "mode":"1",
            "value":"fake_html"
          },
          "msg":"foo"
        })";
        response->body().add(body);
        callbacks.onSuccess(*request_, std::move(response));
        return request_.get();
      }));

  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "Insert", _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        db_v3::InsertResponse response_message;
        response_message.set_result(1);
        response_message.set_user_data(255);
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }));
  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "ShouldRateLimit", _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        ratelimit_v3::RateLimitResponse response_message;
        response_message.set_status(ratelimit_v3::RateLimitResponse_Status_OVER_LIMIT);
        response_message.set_duration(30);
        response_message.set_remaining(0);
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }));
  Grpc::RawAsyncRequestCallbacks* cb{nullptr};
  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "Get", _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        db_v3::GetResponse response_message;
        response_message.set_result(0);
        response_message.set_user_data(255);
        response_message.set_value("foo");
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        cb = &callbacks;
        return grpc_async_client_->async_request_.get();
      }));

  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  db_v3::GetResponse response_message;
  response_message.set_result(0);
  response_message.set_user_data(255);
  response_message.set_value("foo");
  std::string serialized_data;
  response_message.SerializeToString(&serialized_data);
  std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
  buffer->add(serialized_data);
  cb->onSuccessRaw(std::move(buffer), span_);
}

// 紧急模式且立即人机验证
TEST_F(AntiCCTest, Emergency) {
  std::string global_yaml = R"(
  mode: EMERGENCY
  action:
    all_run_verification: true
    man_machine_verification: true
    check_quota:
      duration: 30
      max_count: 3
    rate_limit_quota:
      duration: 30
      max_count: 3
    block_time: 500
  external_grpc_server:
    envoy_grpc:
      cluster_name: grpc_service
    timeout: 1s
  verification_server:
    cluster_name: verify_service
    timeout: 1s
  polycube_server:
    cluster_name: polycube_service
    timeout: 1s
  )";
  setup(global_yaml);
  setAddressToReturn("tcp://10.200.200.121:20000");

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::OK, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)> modify_headers,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::OK, code);
        EXPECT_EQ("fake_html", body);

        Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
        modify_headers(response_headers);
        EXPECT_TRUE(response_headers.has(Http::LowerCaseString("content-type")));

        EXPECT_EQ(grpc_status, absl::nullopt);
        EXPECT_EQ(details, "");
      }));
  EXPECT_CALL(http_async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        Http::ResponseMessagePtr response(new Http::ResponseMessageImpl(
            Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status",
            "200"}}}));
        response->headers().setContentType(Http::Headers::get().ContentTypeValues.Json);
        std::string body = R"(
        {
          "code": 200,
          "data":
          {
            "mode":"1",
            "value":"fake_html"
          },
          "msg":"foo"
        })";
        response->body().add(body);
        callbacks.onSuccess(*request_, std::move(response));
        return request_.get();
      }));

  EXPECT_CALL(*grpc_async_client_, sendRaw(_, _, _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        db_v3::GetResponse response_message;
        response_message.set_result(0);
        response_message.set_user_data(255);
        response_message.set_value("foo");
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        ratelimit_v3::RateLimitResponse response_message;
        response_message.set_status(ratelimit_v3::RateLimitResponse_Status_OK);
        response_message.set_duration(30);
        response_message.set_remaining(0);
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }));

  auto headers = Http::TestRequestHeaderMapImpl();
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
}

// 验证获取验证码
TEST_F(AntiCCTest, GetCode) {
  std::string global_yaml = R"(
  mode: CUSTOM
  action:
    man_machine_verification: true
    check_quota:
      duration: 30
      max_count: 3
    rate_limit_quota:
      duration: 30
      max_count: 3
    block_time: 500
  external_grpc_server:
    envoy_grpc:
      cluster_name: grpc_service
    timeout: 1s
  verification_server:
    cluster_name: verify_service
    timeout: 1s
  polycube_server:
    cluster_name: polycube_service
    timeout: 1s
  )";
  setup(global_yaml);
  setAddressToReturn("tcp://10.200.200.121:20000");

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::OK, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)> modify_headers,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::OK, code);
        EXPECT_EQ("fake_code", body);

        Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
        modify_headers(response_headers);

        EXPECT_EQ(grpc_status, absl::nullopt);
        EXPECT_EQ(details, "");
      }));

  EXPECT_CALL(http_async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        Http::ResponseMessagePtr response(new Http::ResponseMessageImpl(
            Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status",
            "200"}}}));
        response->headers().setContentType(Http::Headers::get().ContentTypeValues.Json);
        std::string body = R"(
        {
          "code": 200,
          "data":
          {
            "mode":"2",
            "value":"fake_code"
          },
          "msg":"foo"
        })";
        response->body().add(body);
        callbacks.onSuccess(*request_, std::move(response));
        return request_.get();
      }));

  Http::TestRequestHeaderMapImpl headers{{"Stone-Rhino-Code-Verify", CODE_GET_CODE}};
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
}

// 获取验证码结构Error
TEST_F(AntiCCTest, GetCodeError) {
  std::string global_yaml = R"(
  mode: CUSTOM
  action:
    man_machine_verification: true
    check_quota:
      duration: 30
      max_count: 3
    rate_limit_quota:
      duration: 30
      max_count: 3
    block_time: 500
  external_grpc_server:
    envoy_grpc:
      cluster_name: grpc_service
    timeout: 1s
  verification_server:
    cluster_name: verify_service
    timeout: 1s
  polycube_server:
    cluster_name: polycube_service
    timeout: 1s
  )";
  setup(global_yaml);
  setAddressToReturn("tcp://10.200.200.121:20000");

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::InternalServerError, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)>,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::InternalServerError, code);
        EXPECT_EQ("", body);
        EXPECT_EQ(absl::nullopt, grpc_status);
        EXPECT_EQ("service_error", details);
      }));

  EXPECT_CALL(http_async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        Http::ResponseMessagePtr response(new Http::ResponseMessageImpl(
            Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status",
            "200"}}}));
        response->headers().setContentType(Http::Headers::get().ContentTypeValues.Json);
        std::string body = R"(
        {
          "code": 200,
          "data":
          {
            "fake_mode": 2,
            "value":"fake_code"
          },
          "msg":"foo"
        })";
        response->body().add(body);
        callbacks.onSuccess(*request_, std::move(response));
        return request_.get();
      }));

  Http::TestRequestHeaderMapImpl headers{{"Stone-Rhino-Code-Verify", CODE_GET_CODE}};
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
}

// 获取验证码请求超时
TEST_F(AntiCCTest, GetCodeTimeout) {
  std::string global_yaml = R"(
  mode: CUSTOM
  action:
    man_machine_verification: true
    check_quota:
      duration: 30
      max_count: 3
    rate_limit_quota:
      duration: 30
      max_count: 3
    block_time: 500
  external_grpc_server:
    envoy_grpc:
      cluster_name: grpc_service
    timeout: 1s
  verification_server:
    cluster_name: verify_service
    timeout: 1s
  polycube_server:
    cluster_name: polycube_service
    timeout: 1s
  )";
  setup(global_yaml);
  setAddressToReturn("tcp://10.200.200.121:20000");

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::RequestTimeout, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)>,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::RequestTimeout, code);
        EXPECT_EQ("", body);
        EXPECT_EQ(absl::nullopt, grpc_status);
        EXPECT_EQ("", details);
      }));

  EXPECT_CALL(http_async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        callbacks.onFailure(*request_, Http::AsyncClient::FailureReason::Reset);
        return request_.get();
      }));

  Http::TestRequestHeaderMapImpl headers{{"Stone-Rhino-Code-Verify", CODE_GET_CODE}};
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
  filter_->onStreamComplete();
  filter_->onDestroy();
}

// 验证成功
TEST_F(AntiCCTest, VerifyCodeSuccess) {
  std::string global_yaml = R"(
  mode: CUSTOM
  action:
    man_machine_verification: true
    check_quota:
      duration: 30
      max_count: 3
    rate_limit_quota:
      duration: 30
      max_count: 3
    block_time: 500
  external_grpc_server:
    envoy_grpc:
      cluster_name: grpc_service
    timeout: 1s
  verification_server:
    cluster_name: verify_service
    timeout: 1s
  polycube_server:
    cluster_name: polycube_service
    timeout: 1s
  )";
  setup(global_yaml);
  setAddressToReturn("tcp://10.200.200.121:20000");

  Http::TestRequestHeaderMapImpl headers{{"Stone-Rhino-Code-Verify", CODE_VERIFY_CODE}};
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));

  std::string body = R"(
  {
    "code" : "df12",
    "src_ip" : "10.200.200.121"
  })";
  Buffer::OwnedImpl buffer(body);
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillOnce(Return(&buffer));

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::OK, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)>,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::OK, code);
        EXPECT_EQ(VERIFY_RESULT_SUCCESS, body);
        EXPECT_EQ(grpc_status, absl::nullopt);
        EXPECT_EQ(details, "clean_quota_success");
      }));
  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "CleanQuota", _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        ratelimit_v3::CleanQuotaResponse response_message;
        response_message.set_result(ratelimit_v3::CleanQuotaResponse_Result_SUCCESS);
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }));

  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "Delete", _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        db_v3::DeleteResponse response_message;
        response_message.set_result(0);
        response_message.set_user_data(255);
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }));
  EXPECT_CALL(http_async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        Http::ResponseMessagePtr response(new Http::ResponseMessageImpl(
            Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status",
            "200"}}}));
        response->headers().setContentType(Http::Headers::get().ContentTypeValues.Json);
        std::string body = R"(
        {
          "code": 200,
          "data":
          {
            "mode":"3",
            "value":"1"
          },
          "msg":"foo"
        })";
        response->body().add(body);
        callbacks.onSuccess(*request_, std::move(response));
        return request_.get();
      }));

  EXPECT_EQ(filter_->decodeData(buffer, true),
  Http::FilterDataStatus::StopIterationAndWatermark);
}

// 验证成功后，清理配额超时
TEST_F(AntiCCTest, CleanQuotaTimeout) {
  std::string global_yaml = R"(
  mode: CUSTOM
  action:
    man_machine_verification: true
    check_quota:
      duration: 30
      max_count: 3
    rate_limit_quota:
      duration: 30
      max_count: 3
    block_time: 500
  external_grpc_server:
    envoy_grpc:
      cluster_name: grpc_service
    timeout: 1s
  verification_server:
    cluster_name: verify_service
    timeout: 1s
  polycube_server:
    cluster_name: polycube_service
    timeout: 1s
  )";
  setup(global_yaml);
  setAddressToReturn("tcp://10.200.200.121:20000");

  Http::TestRequestHeaderMapImpl headers{{"Stone-Rhino-Code-Verify", CODE_VERIFY_CODE}};
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));

  std::string body = R"(
  {
    "code" : "df12",
    "src_ip" : "10.200.200.121"
  })";
  Buffer::OwnedImpl buffer(body);
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillOnce(Return(&buffer));

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::OK, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)>,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::OK, code);
        EXPECT_EQ(VERIFY_RESULT_FAILURE, body);
        EXPECT_EQ(grpc_status, absl::nullopt);
        EXPECT_EQ(details, "clean_quota_failure");
      }));
  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "CleanQuota", _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        callbacks.onFailure(Grpc::Status::WellKnownGrpcStatus::Unavailable,
                            "The RPC endpoint is current unavailable", span_);
        return grpc_async_client_->async_request_.get();
      }));

  EXPECT_CALL(*grpc_async_client_, sendRaw(_, "Delete", _, _, _, _))
      .WillOnce(Invoke([&](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                           Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                           const Http::AsyncClient::RequestOptions&) {
        db_v3::DeleteResponse response_message;
        response_message.set_result(0);
        response_message.set_user_data(255);
        std::string serialized_data;
        response_message.SerializeToString(&serialized_data);
        std::unique_ptr<Buffer::OwnedImpl> buffer = std::make_unique<Buffer::OwnedImpl>();
        buffer->add(serialized_data);
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return grpc_async_client_->async_request_.get();
      }));
  EXPECT_CALL(http_async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        Http::ResponseMessagePtr response(new Http::ResponseMessageImpl(
            Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status",
            "200"}}}));
        response->headers().setContentType(Http::Headers::get().ContentTypeValues.Json);
        std::string body = R"(
        {
          "code": 200,
          "data":
          {
            "mode":"3",
            "value":"1"
          },
          "msg":"foo"
        })";
        response->body().add(body);
        callbacks.onSuccess(*request_, std::move(response));
        return request_.get();
      }));

  EXPECT_EQ(filter_->decodeData(buffer, true),
  Http::FilterDataStatus::StopIterationAndWatermark);
}

// 验证失败
TEST_F(AntiCCTest, VerifyCodeFailure) {
  std::string global_yaml = R"(
  mode: CUSTOM
  action:
    man_machine_verification: true
    check_quota:
      duration: 30
      max_count: 3
    rate_limit_quota:
      duration: 30
      max_count: 3
    block_time: 500
  external_grpc_server:
    envoy_grpc:
      cluster_name: grpc_service
    timeout: 1s
  verification_server:
    cluster_name: verify_service
    timeout: 1s
  polycube_server:
    cluster_name: polycube_service
    timeout: 1s
  )";
  setup(global_yaml);
  setAddressToReturn("tcp://10.200.200.121:20000");

  Http::TestRequestHeaderMapImpl headers{{"Stone-Rhino-Code-Verify", CODE_VERIFY_CODE}};
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));

  std::string body = R"(
  {
    "code" : "df12",
    "src_ip" : "10.200.200.121"
  })";
  Buffer::OwnedImpl buffer(body);
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillOnce(Return(&buffer));

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::OK, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)>,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::OK, code);
        EXPECT_EQ(VERIFY_RESULT_FAILURE, body);
        EXPECT_EQ(absl::nullopt, grpc_status);
        EXPECT_EQ("", details);
      }));
  EXPECT_CALL(http_async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        Http::ResponseMessagePtr response(new Http::ResponseMessageImpl(
            Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status",
            "200"}}}));
        response->headers().setContentType(Http::Headers::get().ContentTypeValues.Json);
        std::string body = R"(
        {
          "code": 200,
          "data":
          {
            "mode":"3",
            "value":"2"
          },
          "msg":"foo"
        })";
        response->body().add(body);
        callbacks.onSuccess(*request_, std::move(response));
        return request_.get();
      }));

  EXPECT_EQ(filter_->decodeData(buffer, true),
  Http::FilterDataStatus::StopIterationAndWatermark);
}

// 验证码结构Error，返回403
TEST_F(AntiCCTest, InvalidCode) {
  std::string global_yaml = R"(
  mode: CUSTOM
  action:
    man_machine_verification: true
    check_quota:
      duration: 30
      max_count: 3
    rate_limit_quota:
      duration: 30
      max_count: 3
    block_time: 500
  external_grpc_server:
    envoy_grpc:
      cluster_name: grpc_service
    timeout: 1s
  verification_server:
    cluster_name: verify_service
    timeout: 1s
  polycube_server:
    cluster_name: polycube_service
    timeout: 1s
  )";
  setup(global_yaml);
  setAddressToReturn("tcp://10.200.200.121:20000");

  Http::TestRequestHeaderMapImpl headers{{"Stone-Rhino-Code-Verify", CODE_VERIFY_CODE}};
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));

  std::string body = "fake_body";
  Buffer::OwnedImpl buffer(body);
  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).WillOnce(Return(&buffer));

  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Http::Code::Forbidden, _, _, _, _))
      .WillOnce(Invoke([](Http::Code code, absl::string_view body,
                          std::function<void(Http::ResponseHeaderMap & headers)>,
                          const absl::optional<Grpc::Status::GrpcStatus> grpc_status,
                          absl::string_view details) {
        EXPECT_EQ(Http::Code::Forbidden, code);
        EXPECT_EQ("", body);
        EXPECT_EQ(absl::nullopt, grpc_status);
        EXPECT_EQ("json_parse_error", details);
      }));
  EXPECT_EQ(filter_->decodeData(buffer, true),
  Http::FilterDataStatus::StopIterationAndWatermark);
}

// 恶意构造验证URL，拒绝
TEST_F(AntiCCTest, InvalidVerifyURL) {
  std::string global_yaml = R"(
  mode: CUSTOM
  action:
    man_machine_verification: true
    check_quota:
      duration: 30
      max_count: 3
    rate_limit_quota:
      duration: 30
      max_count: 3
    block_time: 500
  external_grpc_server:
    envoy_grpc:
      cluster_name: grpc_service
    timeout: 1s
  verification_server:
    cluster_name: verify_service
    timeout: 1s
  polycube_server:
    cluster_name: polycube_service
    timeout: 1s
  )";
  setup(global_yaml);
  setAddressToReturn("tcp://10.200.200.121:20000");
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
  Http::TestRequestHeaderMapImpl headers{{"Stone-Rhino-Code-Verify", "5"}};
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, filter_->decodeHeaders(headers, false));
}

} // namespace AntiCC
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy