#include "test/mocks/server/mocks.h"
#include "test/mocks/local_info/mocks.h"
#include "source/common/network/address_impl.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "filters/source/extensions/filters/http/strong_stateful_session/config.h"
#include "filters/source/extensions/filters/http/strong_stateful_session/filter.h"
#include "filters/source/extensions/filters/http/strong_stateful_session/impl/cookie.h"
#include "filters/source/extensions/filters/http/common/central_database/database.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {

namespace dbv3 = envoy::extensions::filters::http::common::central_database::v3;

std::string Impl::test_parseSetCookieValue(const absl::string_view& cookie_header_value,
                                           const std::string& name) {
  return Impl::Cookie::parseSetCookieValue(cookie_header_value, name);
}

class StrongStatefulSessionTest : public testing::Test, public Logger::Loggable<Logger::Id::filter> {
public:
  StrongStatefulSessionTest() = default;

  void setup(const std::string& yaml) {
    v3::StrongStatefulSessionGlobal proto;
    TestUtility::loadFromYamlAndValidate(yaml, proto);

    // 模拟中央会话缓存
    if (proto.has_grpc_service()) {
      ON_CALL(grpc_context_, clusterManager()).WillByDefault(ReturnRef(cluster_manager_));
      ON_CALL(cluster_manager_, grpcAsyncClientManager()).WillByDefault(ReturnRef(client_manager_));
      async_client_ = std::make_shared<Grpc::MockAsyncClient>();
      ON_CALL(client_manager_, getOrCreateRawAsyncClient(_, _, _, _))
          .WillByDefault(Return(async_client_));
      EXPECT_CALL(request_, cancel()).WillRepeatedly(Invoke([]() -> void {
        ENVOY_LOG(trace, "cancel");
      }));
      EXPECT_CALL(*async_client_, sendRaw(_, _, _, _, _, _))
          .WillRepeatedly(
              Invoke([&](absl::string_view /*service_full_name*/, absl::string_view method_name,
                         Buffer::InstancePtr&& message, Grpc::RawAsyncRequestCallbacks& cb,
                         Tracing::Span& span,
                         const Http::AsyncClient::RequestOptions& options) -> Grpc::AsyncRequest* {
                EXPECT_EQ(options.timeout->count(), 5000);
                message_ = std::move(message);
                method_name_ = method_name;
                callbacks_ = &cb;
                parent_span_ = &span;
                ENVOY_LOG(trace, "method_name:{}", method_name);

                return &request_;
              }));

      const std::chrono::milliseconds timeout =
          std::chrono::milliseconds(PROTOBUF_GET_MS_OR_DEFAULT(proto.grpc_service(), timeout, 20));
      central_db_.reset(new Filters::Common::CentralDatabase::Database(
          grpc_context_, proto.grpc_service(), timeout, FILTER_NAME));
    }

    // 创建全局配置，由于构造函数中会调用grpc，因此当配置了grpc时，要做一次gprc的模拟调用
    global_config_ = std::make_shared<FilterGlobalConfig>(proto, grpc_context_);
    if (proto.has_grpc_service()) {
      dealGrpcCall();
    }

    filter_ = std::make_shared<Filter>(global_config_, context_, std::move(central_db_));
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    filter_->setEncoderFilterCallbacks(encoder_callbacks_);
    ON_CALL(decoder_callbacks_, streamInfo()).WillByDefault(ReturnRef(stream_info_));
    // EXPECT_CALL(decoder_callbacks_,
    // setUpstreamOverrideHost(testing::_)).WillRepeatedly(testing::SaveArg<0>(&overrideHost_));
    // 当setUpstreamOverrideHost函数被调用时，模拟修改上游服务器IP
    EXPECT_CALL(decoder_callbacks_, setUpstreamOverrideHost(testing::_))
        .WillRepeatedly(testing::Invoke([this](auto arg) {
          std::string upstream_address(arg);
          this->setUpstreamAddress(upstream_address);
        }));
    ON_CALL(encoder_callbacks_, streamInfo()).WillByDefault(ReturnRef(stream_info_));

    // 模拟上游服务器
    upstream_host_ = std::make_shared<NiceMock<Envoy::Upstream::MockHostDescription>>();
    stream_info_.upstreamInfo()->setUpstreamHost(upstream_host_);
    upstream_address_ = Network::Utility::parseInternetAddress("10.1.2.3", 679, false); // default
    EXPECT_CALL(*upstream_host_, address()).WillRepeatedly(Return(upstream_address_));

    // 模拟ssl
    downstream_ssl_info_ = std::make_shared<NiceMock<Ssl::MockConnectionInfo>>();
    stream_info_.downstream_connection_info_provider_->setSslConnection(downstream_ssl_info_);
  }

  void setDownstreamAddress(const std::string& address, uint32_t port) {
    stream_info_.downstream_connection_info_provider_->setRemoteAddress(
        std::make_unique<Network::Address::Ipv4Instance>(address, port));
  }

  void setUpstreamAddress(const std::string& upstream_ip_address) {
    upstream_address_ = Network::Utility::parseInternetAddressAndPort(upstream_ip_address);
    EXPECT_CALL(*upstream_host_, address()).WillRepeatedly(Return(upstream_address_));
  }

  void setSslSessionId(const std::string& session_id) {
    ssl_session_id_ = session_id;
    EXPECT_CALL(*downstream_ssl_info_, sessionId())
        .WillRepeatedly(testing::ReturnRef(ssl_session_id_));
  }

  const std::string& getUpstreamAddress() const {
    return stream_info_.upstreamInfo()->upstreamHost()->address()->asString();
  }

  // 模拟负载均衡，轮询方式
  const std::string& getRandVirtualHost() const {
    std::srand(std::time(0));
    static size_t virtual_index = rand();
    virtual_index = (virtual_index + 1) % virtual_host_.size();
    return virtual_host_[virtual_index];
  }

  // 模拟中央会话缓存grpc调用
  void dealGrpcCall() {
      EXPECT_NE(nullptr, callbacks_);
      // EXPECT_EQ("Get", method_name_);

      dbv3::GetRequest get_request;
      dbv3::GetResponse get_response;
      get_response.Clear();

      if (method_name_ == "Clean") {
        cleanCentralCache();
        get_response.set_result(1);
      } else if (method_name_ == "Get") {
        EXPECT_TRUE(Grpc::Common::parseBufferInstance(std::move(message_), get_request));
        if (get_request.value().size() == sizeof(Impl::UpstreamInfo)) {
          ENVOY_LOG(trace, "get with data");
          central_cache_.access(
              get_request.key(),
              [&](Impl::UpstreamInfo& upstream) {
                upstream =
                    *(reinterpret_cast<const Impl::UpstreamInfo*>(get_request.value().c_str()));
                ENVOY_LOG(trace, "update upstream ip: {}", upstream.ip_);

                // for coverage test:  make true  if (*remote_cache_upstream != upstream) {
                upstream.time_stamp_ -= 1;

                get_response.set_result(1);
                get_response.set_value(&upstream, sizeof(Impl::UpstreamInfo));
              },
              [&]() {
                const Impl::UpstreamInfo* upstream =
                    reinterpret_cast<const Impl::UpstreamInfo*>(get_request.value().c_str());
                ENVOY_LOG(trace, "insert upstream into central cache. ip_: {}, time_stamp_: {}", upstream->ip_, upstream->time_stamp_);
                get_response.set_result(2);
                return *(upstream);
              });
        } else {
          ENVOY_LOG(trace, "Read");

          central_cache_.peek(get_request.key(), [&](const Impl::UpstreamInfo* upstream) {
            if (upstream) {
              get_response.set_result(1);
              get_response.set_value(upstream, sizeof(Impl::UpstreamInfo));
              ENVOY_LOG(trace, "read upstream from central cache. ip_: {}, timestamp: {}", upstream->ip_, upstream->time_stamp_);
            } else {
              get_response.set_result(0);
              ENVOY_LOG(trace, "read failed");
            }
          });
        }
      } else {
        ENVOY_LOG(trace, "Unknown method: {}", method_name_);
      }

      callbacks_->onSuccessRaw(Grpc::Common::serializeMessage(get_response), *parent_span_);

      // prepare for next call
      callbacks_ = nullptr;
  }

  void dealHeadersStatus(Http::FilterHeadersStatus status) {
    if (status == Http::FilterHeadersStatus::StopAllIterationAndWatermark) { // need wait grpc
      dealGrpcCall();
    } else {
      EXPECT_EQ(Http::FilterHeadersStatus::Continue, status);
    }
  }

  void cleanCentralCache() {
    central_cache_.clean(central_cache_.size(), [](const Impl::UpstreamInfo&) { return true; });
  }

protected:
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info_;
  NiceMock<Event::MockDispatcher> dispatcher_;
  NiceMock<Stats::MockIsolatedStatsStore> stats_;
  NiceMock<Server::Configuration::MockServerFactoryContext> context_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_callbacks_;
  std::shared_ptr<NiceMock<Envoy::Upstream::MockHostDescription>> upstream_host_;
  Network::Address::InstanceConstSharedPtr upstream_address_;
  std::shared_ptr<NiceMock<Ssl::MockConnectionInfo>> downstream_ssl_info_;
  std::string ssl_session_id_;

  std::shared_ptr<FilterGlobalConfig> global_config_;
  std::shared_ptr<Filter> filter_;
  absl::string_view overrideHost_;
  std::vector<std::string> virtual_host_ = {
      "10.10.1.1:4234",
      "10.10.1.2:3235",
      "10.10.1.3:1244",
      "10.10.1.4:1439",
  };

  std::unique_ptr<Filters::Common::CentralDatabase::Database> central_db_;
  NiceMock<Server::Configuration::MockFactoryContext> grpc_context_;
  NiceMock<Upstream::MockClusterManager> cluster_manager_;
  NiceMock<Grpc::MockAsyncClientManager> client_manager_;
  std::shared_ptr<Grpc::MockAsyncClient> async_client_;
  NiceMock<Grpc::MockAsyncRequest> request_;

  Filters::Common::LruCache<std::string /*key*/, Impl::UpstreamInfo /*upstream Info*/,
                            20011 /*must be prime number*/>
      central_cache_;
  absl::string_view method_name_;
  Buffer::InstancePtr message_;
  Grpc::RawAsyncRequestCallbacks* callbacks_{nullptr};
  Tracing::Span* parent_span_{nullptr};
};

// 测试source_ip
TEST_F(StrongStatefulSessionTest, TestSourceIp) {
  const std::string yaml = R"(
      src_ip:
        prefix_len: 24
      ttl: 5s
      grpc_service:
        envoy_grpc:
          cluster_name: grpc_service
        timeout: 5s
    )";
  setup(yaml);

  // 测试源IP属于同一会话的情况
  {
    std::vector<std::string> downstream_ip = {"192.168.1.5", "192.168.1.6", "192.168.1.12"};

    std::string upstream_ip;
    for (auto ip : downstream_ip) {
      ENVOY_LOG(trace, "downstream ip: {}", ip);
      setDownstreamAddress(ip, 1234);

      setUpstreamAddress(getRandVirtualHost());
      ENVOY_LOG(trace, "real upstream ip: {}", getUpstreamAddress());

      Http::TestRequestHeaderMapImpl request{
          {":path", "/anything?aa=bb&cc=dd"},
          {":method", "GET"},
          {":scheme", "http"},
          {"user-agent", ""},
          {"accept", "*/*"},
          {"x-forwarded-proto", "http"},
          {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
          {"x-envoy-expected-rq-timeout-ms", "15000"},
      };

      dealHeadersStatus(filter_->decodeHeaders(request, false));

      Http::TestResponseHeaderMapImpl response;
      dealHeadersStatus(filter_->encodeHeaders(response, false));

      if (upstream_ip.empty()) {
        upstream_ip = getUpstreamAddress();
      } else {
        EXPECT_EQ(upstream_ip, getUpstreamAddress());
      }
    }
  }

  // 测试源IP不属于同一会话的情况，由于会话超时引起
  {
    // 确保已有缓存全部过期
    cleanCentralCache();
    sleep(6);

    std::vector<std::string> downstream_ip = {"192.168.1.5", "192.168.1.6", "192.168.1.12"};

    std::string upstream_ip;
    for (auto ip : downstream_ip) {
      setDownstreamAddress(ip, 1234);

      setUpstreamAddress(getRandVirtualHost());

      Http::TestRequestHeaderMapImpl request{
          {":path", "/anything?aa=bb&cc=dd"},
          {":method", "GET"},
          {":scheme", "http"},
          {"user-agent", ""},
          {"accept", "*/*"},
          {"x-forwarded-proto", "http"},
          {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
          {"x-envoy-expected-rq-timeout-ms", "15000"},
      };

      dealHeadersStatus(filter_->decodeHeaders(request, false));

      Http::TestResponseHeaderMapImpl response;
      dealHeadersStatus(filter_->encodeHeaders(response, false));
      // 等待会话超时
      sleep(6);

      if (upstream_ip.empty()) {
        upstream_ip = getUpstreamAddress();
      } else {
        EXPECT_NE(upstream_ip, getUpstreamAddress());
      }
    }
  }

  // 测试源IP不属于同一会话的情况，由于源IP不同引起
  {
    // 确保已有缓存全部过期
    cleanCentralCache();
    sleep(6);

    std::vector<std::string> downstream_ip = {"192.168.6.5", "192.168.2.6", "192.168.3.12"};

    std::string upstream_ip;
    for (auto ip : downstream_ip) {
      setDownstreamAddress(ip, 1234);

      setUpstreamAddress(getRandVirtualHost());

      Http::TestRequestHeaderMapImpl request{
          {":path", "/anything?aa=bb&cc=dd"},
          {":method", "GET"},
          {":scheme", "http"},
          {"user-agent", ""},
          {"accept", "*/*"},
          {"x-forwarded-proto", "http"},
          {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
          {"x-envoy-expected-rq-timeout-ms", "15000"},
      };
      dealHeadersStatus(filter_->decodeHeaders(request, false));

      Http::TestResponseHeaderMapImpl response;
      dealHeadersStatus(filter_->encodeHeaders(response, false));

      if (upstream_ip.empty()) {
        upstream_ip = getUpstreamAddress();
      } else {
        EXPECT_NE(upstream_ip, getUpstreamAddress());
      }
    }
  }

  filter_->onDestroy();
}

// 测试 http header
TEST_F(StrongStatefulSessionTest, TestHttpHeader) {
  const std::string yaml = R"(
      header:
        name: lb-rhino
      ttl: 5s
      grpc_service:
        envoy_grpc:
          cluster_name: grpc_service
        timeout: 5s
    )";
  const Http::LowerCaseString headerKey("lb-rhino");
  setup(yaml);

  // 测试 header相同属于同一会话的情况
  {
    std::vector<std::string> downstream_headers = {
        "1234567",
        "1234567",
    };

    std::string upstream_ip;
    for (auto header : downstream_headers) {
      setUpstreamAddress(getRandVirtualHost());

      Http::TestRequestHeaderMapImpl request{
          {":authority", "localhost:8888"},
          {":path", "/anything?aa=bb&cc=dd"},
          {":method", "GET"},
          {":scheme", "http"},
          {"user-agent", ""},
          {"host", "localhost"},
          {"accept", "*/*"},
          {"x-forwarded-proto", "http"},
          {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
          {"x-envoy-expected-rq-timeout-ms", "15000"},
          {"foo", "bar"},
          {"end-user", "envoy"},
      };
      request.addReferenceKey(headerKey, header);
      dealHeadersStatus(filter_->decodeHeaders(request, false));

      Http::TestResponseHeaderMapImpl response;
      dealHeadersStatus(filter_->encodeHeaders(response, false));

      if (upstream_ip.empty()) {
        upstream_ip = getUpstreamAddress();
      } else {
        EXPECT_EQ(upstream_ip, getUpstreamAddress());
      }
    }
  }

  // 测试 header不相同不属于同一会话的情况
  {
    std::vector<std::string> downstream_headers = {"9234567", "81234567", ""};

    std::string upstream_ip;
    for (auto header : downstream_headers) {
      setUpstreamAddress(getRandVirtualHost());

      Http::TestRequestHeaderMapImpl request{
          {":authority", "localhost:8888"},
          {":path", "/anything?aa=bb&cc=dd"},
          {":method", "GET"},
          {":scheme", "http"},
          {"user-agent", ""},
          {"accept", "*/*"},
          {"x-forwarded-proto", "http"},
          {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
          {"x-envoy-expected-rq-timeout-ms", "15000"},
          {"foo", "bar"},
          {"end-user", "envoy"},
      };
      if (!header.empty()) {
        request.addReferenceKey(headerKey, header);
      }
      dealHeadersStatus(filter_->decodeHeaders(request, false));

      Http::TestResponseHeaderMapImpl response;
      dealHeadersStatus(filter_->encodeHeaders(response, false));

      if (upstream_ip.empty()) {
        upstream_ip = getUpstreamAddress();
      } else {
        EXPECT_NE(upstream_ip, getUpstreamAddress());
      }
    }
  }
}

// 测试 http header，含有大写字母的情况
TEST_F(StrongStatefulSessionTest, TestHttpHeaderUppercase) {
  const std::string yaml = R"(
      header:
        name: User-Agent
      ttl: 5s
      grpc_service:
        envoy_grpc:
          cluster_name: grpc_service
        timeout: 5s
    )";
  const Http::LowerCaseString headerKey("User-Agent");
  setup(yaml);

  // 测试 header相同属于同一会话的情况
  {
    std::vector<std::string> downstream_headers = {
        "firefox",
        "firefox",
    };

    std::string upstream_ip;
    for (auto header : downstream_headers) {
      setUpstreamAddress(getRandVirtualHost());

      Http::TestRequestHeaderMapImpl request{
          {":authority", "localhost:8888"},
          {":path", "/anything?aa=bb&cc=dd"},
          {":method", "GET"},
          {":scheme", "http"},
          {"host", "localhost"},
          {"accept", "*/*"},
          {"x-forwarded-proto", "http"},
          {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
          {"x-envoy-expected-rq-timeout-ms", "15000"},
          {"foo", "bar"},
          {"end-user", "envoy"},
      };
      request.addReferenceKey(headerKey, header);
      dealHeadersStatus(filter_->decodeHeaders(request, false));

      Http::TestResponseHeaderMapImpl response;
      dealHeadersStatus(filter_->encodeHeaders(response, false));

      if (upstream_ip.empty()) {
        upstream_ip = getUpstreamAddress();
      } else {
        EXPECT_EQ(upstream_ip, getUpstreamAddress());
      }
    }
  }

  // 测试 header不相同不属于同一会话的情况
  {
    std::vector<std::string> downstream_headers = {
        "firefox",
        "chrome",
    };

    std::string upstream_ip;
    for (auto header : downstream_headers) {
      setUpstreamAddress(getRandVirtualHost());

      Http::TestRequestHeaderMapImpl request{
          {":authority", "localhost:8888"},
          {":path", "/anything?aa=bb&cc=dd"},
          {":method", "GET"},
          {":scheme", "http"},
          {"host", "localhost"},
          {"accept", "*/*"},
          {"x-forwarded-proto", "http"},
          {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
          {"x-envoy-expected-rq-timeout-ms", "15000"},
          {"foo", "bar"},
          {"end-user", "envoy"},
      };
      request.addReferenceKey(headerKey, header);
      dealHeadersStatus(filter_->decodeHeaders(request, false));

      Http::TestResponseHeaderMapImpl response;
      dealHeadersStatus(filter_->encodeHeaders(response, false));

      if (upstream_ip.empty()) {
        upstream_ip = getUpstreamAddress();
      } else {
        EXPECT_NE(upstream_ip, getUpstreamAddress());
      }
    }
  }
}

// 测试 ssl session
TEST_F(StrongStatefulSessionTest, TestSslSession) {
  const std::string yaml = R"(
      ssl_session: {}
      ttl: 5s
      grpc_service:
        envoy_grpc:
          cluster_name: grpc_service
        timeout: 5s
    )";
  setup(yaml);

  // 测试 session id相同，属于同一会话的情况
  {
    std::vector<std::string> downstream_session_ids = {
        "1234567",
        "1234567",
    };

    std::string upstream_ip;
    for (auto sessionId : downstream_session_ids) {
      setUpstreamAddress(getRandVirtualHost());
      setSslSessionId(sessionId);

      Http::TestRequestHeaderMapImpl request{
          {":authority", "localhost:8888"},
          {":path", "/anything?aa=bb&cc=dd"},
          {":method", "GET"},
          {":scheme", "https"},
          {"user-agent", ""},
          {"accept", "*/*"},
          {"x-forwarded-proto", "http"},
          {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
          {"x-envoy-expected-rq-timeout-ms", "15000"},
          {"foo", "bar"},
          {"end-user", "envoy"},
      };
      dealHeadersStatus(filter_->decodeHeaders(request, false));

      Http::TestResponseHeaderMapImpl response;
      dealHeadersStatus(filter_->encodeHeaders(response, false));

      if (upstream_ip.empty()) {
        upstream_ip = getUpstreamAddress();
      } else {
        EXPECT_EQ(upstream_ip, getUpstreamAddress());
      }
    }
  }

  // 测试session id不相同，不属于同一会话的情况
  {
    std::vector<std::string> downstream_session_ids = {
        "9234567",
        "81234567",
    };

    std::string upstream_ip;
    for (auto sessionId : downstream_session_ids) {
      setUpstreamAddress(getRandVirtualHost());
      setSslSessionId(sessionId);

      Http::TestRequestHeaderMapImpl request{
          {":authority", "localhost:8888"},
          {":path", "/anything?aa=bb&cc=dd"},
          {":method", "GET"},
          {":scheme", "https"},
          {"user-agent", ""},
          {"accept", "*/*"},
          {"x-forwarded-proto", "http"},
          {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
          {"x-envoy-expected-rq-timeout-ms", "15000"},
          {"foo", "bar"},
          {"end-user", "envoy"},
      };
      dealHeadersStatus(filter_->decodeHeaders(request, false));

      Http::TestResponseHeaderMapImpl response;
      dealHeadersStatus(filter_->encodeHeaders(response, false));

      if (upstream_ip.empty()) {
        upstream_ip = getUpstreamAddress();
      } else {
        EXPECT_NE(upstream_ip, getUpstreamAddress());
      }
    }
  }
}

// 测试 插入cookie
TEST_F(StrongStatefulSessionTest, TestInsertCookie) {
  const std::string yaml = R"(
      cookie:
        insert: true
        session: true
        name: lb-rhino
        domain: ouryun.com
        path: /
      ttl: 5s
      grpc_service:
        envoy_grpc:
          cluster_name: grpc_service
        timeout: 5s
  )";
  const std::string cookie_name("lb-rhino");
  setup(yaml);

  // 模拟同一客户端：第一个请求的响应会设置set-cookie，后续请求携带此cookie
  {
    // step1 发送第一个请求，不带cookie
    setUpstreamAddress(getRandVirtualHost());
    Http::TestRequestHeaderMapImpl request{
        {":authority", "localhost:8888"},
        {":path", "/anything?aa=bb&cc=dd"},
        {":method", "GET"},
        {":scheme", "https"},
        {"user-agent", ""},
        {"accept", "*/*"},
        {"x-forwarded-proto", "http"},
        {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
        {"x-envoy-expected-rq-timeout-ms", "15000"},
        {"foo", "bar"},
        {"end-user", "envoy"},
    };
    dealHeadersStatus(filter_->decodeHeaders(request, false));

    // step2 确认响应报文被插入了cookie
    Http::TestResponseHeaderMapImpl response;
    dealHeadersStatus(filter_->encodeHeaders(response, false));

    std::string set_cookie_value(Http::Utility::parseSetCookieValue(response, cookie_name));
    EXPECT_TRUE(!set_cookie_value.empty());
    std::string cookie_value(cookie_name + "=" + set_cookie_value);
    ENVOY_LOG(trace, "cookie_value: {}", cookie_value);

    std::string upstream_ip(getUpstreamAddress());

    // step3 其它请求，携带上面插入的cookie
    for (size_t i = 0; i < 3; i++) {
      setUpstreamAddress(getRandVirtualHost());
      Http::TestRequestHeaderMapImpl request{
          {":authority", "localhost:8888"},
          {":path", "/anything?aa=bb&cc=dd"},
          {":method", "GET"},
          {":scheme", "https"},
          {"user-agent", ""},
          {"accept", "*/*"},
          {"x-forwarded-proto", "http"},
          {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
          {"x-envoy-expected-rq-timeout-ms", "15000"},
          {"foo", "bar"},
          {"end-user", "envoy"},
      };
      request.addReferenceKey(Http::Headers::get().Cookie, cookie_value);
      dealHeadersStatus(filter_->decodeHeaders(request, false));

      Http::TestResponseHeaderMapImpl response;
      dealHeadersStatus(filter_->encodeHeaders(response, false));

      // step4 确认所有请求属于同一个会话
      EXPECT_EQ(upstream_ip, getUpstreamAddress());
    }
  }

  // 模拟不同客户端：每个请求都分别是不同客户端发送的第一个请求，即不携带cookie
  {
    std::string upstream_ip;
    for (size_t i = 0; i < 3; i++) {
      setUpstreamAddress(getRandVirtualHost());
      Http::TestRequestHeaderMapImpl request{
          {":authority", "localhost:8888"},
          {":path", "/anything?aa=bb&cc=dd"},
          {":method", "GET"},
          {":scheme", "https"},
          {"user-agent", ""},
          {"accept", "*/*"},
          {"x-forwarded-proto", "http"},
          {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
          {"x-envoy-expected-rq-timeout-ms", "15000"},
          {"foo", "bar"},
          {"end-user", "envoy"},
      };
      dealHeadersStatus(filter_->decodeHeaders(request, false));

      // 确认响应报文被插入了cookie
      Http::TestResponseHeaderMapImpl response;
      dealHeadersStatus(filter_->encodeHeaders(response, false));
      EXPECT_TRUE(!Http::Utility::parseSetCookieValue(response, cookie_name).empty());

      // 确认这些请求被路由至不同的上游服务器
      if (upstream_ip.empty()) {
        upstream_ip = getUpstreamAddress();
      } else {
        EXPECT_NE(upstream_ip, getUpstreamAddress());
      }
    }
  }
}

// 测试 重写cookie
TEST_F(StrongStatefulSessionTest, TestRewriteCookie) {
  const std::string yaml = R"(
      cookie:
        rewrite: true
        session: true
        name: lb-rhino
        domain: ouryun.com
        path: /
      ttl: 5s
      grpc_service:
        envoy_grpc:
          cluster_name: grpc_service
        timeout: 5s
  )";
  const std::string cookie_name("lb-rhino");
  const std::string mock_cookie_value("aabbccddee");
  const std::string magic_prefix_uuid("cfa12fd9968c430480938939af8cea48");
  setup(yaml);

  // 模拟同一客户端：第一个请求的响应会设置set-cookie，后续请求携带此cookie
  {
    // step1 发送第一个请求，不带cookie
    setUpstreamAddress(getRandVirtualHost());
    Http::TestRequestHeaderMapImpl request{
        {":authority", "localhost:8888"},
        {":path", "/anything?aa=bb&cc=dd"},
        {":method", "GET"},
        {":scheme", "https"},
        {"user-agent", ""},
        {"accept", "*/*"},
        {"x-forwarded-proto", "http"},
        {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
        {"x-envoy-expected-rq-timeout-ms", "15000"},
        {"foo", "bar"},
        {"end-user", "envoy"},
    };
    dealHeadersStatus(filter_->decodeHeaders(request, false));

    // step2 在模拟响应包中插入set-cookie
    Http::TestResponseHeaderMapImpl response;
    response.addReferenceKey(Http::Headers::get().SetCookie,
                             Http::Utility::makeSetCookieValue(cookie_name, mock_cookie_value, "/",
                                                               std::chrono::seconds(3600), true));
    dealHeadersStatus(filter_->encodeHeaders(response, false));

    // step3 确认响应报文中原有的cookie被修改了
    std::string set_cookie_value(Http::Utility::parseSetCookieValue(response, cookie_name));
    EXPECT_TRUE(!set_cookie_value.empty());
    std::string cookie_value(cookie_name + "=" + set_cookie_value);
    ENVOY_LOG(trace, "cookie_value: {}", cookie_value);
    EXPECT_TRUE(testing::IsSubstring("", "", magic_prefix_uuid.c_str(), cookie_value));

    std::string upstream_ip(getUpstreamAddress());

    // step4 其它请求，携带上面插入的cookie
    for (size_t i = 0; i < 3; i++) {
      setUpstreamAddress(getRandVirtualHost());
      Http::TestRequestHeaderMapImpl request{
          {":authority", "localhost:8888"},
          {":path", "/anything?aa=bb&cc=dd"},
          {":method", "GET"},
          {":scheme", "https"},
          {"user-agent", ""},
          {"accept", "*/*"},
          {"x-forwarded-proto", "http"},
          {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
          {"x-envoy-expected-rq-timeout-ms", "15000"},
          {"foo", "bar"},
          {"end-user", "envoy"},
      };
      request.addReferenceKey(Http::Headers::get().Cookie, cookie_value);
      dealHeadersStatus(filter_->decodeHeaders(request, false));

      // step5 确认请求中的cookie被还原
      EXPECT_EQ(mock_cookie_value, Http::Utility::parseCookieValue(request, cookie_name));

      Http::TestResponseHeaderMapImpl response;
      dealHeadersStatus(filter_->encodeHeaders(response, false));

      // step6 确认所有请求属于同一个会话
      EXPECT_EQ(upstream_ip, getUpstreamAddress());
    }
  }

  // 模拟不同客户端：每个请求都分别是不同客户端发送的第一个请求，即不携带cookie
  {
    std::string upstream_ip;
    for (size_t i = 0; i < 3; i++) {
      setUpstreamAddress(getRandVirtualHost());
      Http::TestRequestHeaderMapImpl request{
          {":authority", "localhost:8888"},
          {":path", "/anything?aa=bb&cc=dd"},
          {":method", "GET"},
          {":scheme", "https"},
          {"user-agent", ""},
          {"accept", "*/*"},
          {"x-forwarded-proto", "http"},
          {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
          {"x-envoy-expected-rq-timeout-ms", "15000"},
          {"foo", "bar"},
          {"end-user", "envoy"},
      };
      dealHeadersStatus(filter_->decodeHeaders(request, false));

      // 确认响应报文被插入了cookie
      Http::TestResponseHeaderMapImpl response;
      dealHeadersStatus(filter_->encodeHeaders(response, false));
      EXPECT_TRUE(!Http::Utility::parseSetCookieValue(response, cookie_name).empty());

      // 确认这些请求被路由至不同的上游服务器
      if (upstream_ip.empty()) {
        upstream_ip = getUpstreamAddress();
      } else {
        EXPECT_NE(upstream_ip, getUpstreamAddress());
      }
    }
  }
}

// 测试 读取cookie
TEST_F(StrongStatefulSessionTest, TestReadCookie) {
  const std::string yaml = R"(
      cookie:
        read: true
        session: true
        name: lb-rhino
        domain: ouryun.com
        path: /
      ttl: 5s
      grpc_service:
        envoy_grpc:
          cluster_name: grpc_service
        timeout: 5s
  )";
  const std::string cookie_name("lb-rhino");
  const std::string mock_cookie_value("aabbccddee");
  setup(yaml);

  // 模拟同一客户端：第一个请求的响应会设置set-cookie，后续请求携带此cookie
  {
    // step1 发送第一个请求，不带cookie
    setUpstreamAddress(getRandVirtualHost());
    Http::TestRequestHeaderMapImpl request{
        {":authority", "localhost:8888"},
        {":path", "/anything?aa=bb&cc=dd"},
        {":method", "GET"},
        {":scheme", "https"},
        {"user-agent", ""},
        {"accept", "*/*"},
        {"x-forwarded-proto", "http"},
        {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
        {"x-envoy-expected-rq-timeout-ms", "15000"},
        {"foo", "bar"},
        {"end-user", "envoy"},
    };
    dealHeadersStatus(filter_->decodeHeaders(request, false));

    // step2 在模拟响应包中插入set-cookie
    Http::TestResponseHeaderMapImpl response;
    response.addReferenceKey(Http::Headers::get().SetCookie,
                             Http::Utility::makeSetCookieValue(cookie_name, mock_cookie_value, "/",
                                                               std::chrono::seconds(3600), true));
    dealHeadersStatus(filter_->encodeHeaders(response, false));

    // step3 确认cookie未被修改
    EXPECT_EQ(mock_cookie_value, Http::Utility::parseSetCookieValue(response, cookie_name));
    std::string cookie_value(cookie_name + "=" + mock_cookie_value);

    std::string upstream_ip(getUpstreamAddress());

    // step4 其它请求，携带上面插入的cookie
    for (size_t i = 0; i < 3; i++) {
      setUpstreamAddress(getRandVirtualHost());
      Http::TestRequestHeaderMapImpl request{
          {":authority", "localhost:8888"},
          {":path", "/anything?aa=bb&cc=dd"},
          {":method", "GET"},
          {":scheme", "https"},
          {"user-agent", ""},
          {"accept", "*/*"},
          {"x-forwarded-proto", "http"},
          {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
          {"x-envoy-expected-rq-timeout-ms", "15000"},
          {"foo", "bar"},
          {"end-user", "envoy"},
      };
      request.addReferenceKey(Http::Headers::get().Cookie, cookie_value);
      dealHeadersStatus(filter_->decodeHeaders(request, false));

      Http::TestResponseHeaderMapImpl response;
      dealHeadersStatus(filter_->encodeHeaders(response, false));

      // step5 确认所有请求属于同一个会话
      EXPECT_EQ(upstream_ip, getUpstreamAddress());
    }
  }

  // 模拟不同客户端：每个请求都分别是不同客户端发送的第一个请求，即不携带cookie
  {
    std::string upstream_ip;
    for (size_t i = 0; i < 3; i++) {
      setUpstreamAddress(getRandVirtualHost());
      Http::TestRequestHeaderMapImpl request{
          {":authority", "localhost:8888"},
          {":path", "/anything?aa=bb&cc=dd"},
          {":method", "GET"},
          {":scheme", "https"},
          {"user-agent", ""},
          {"accept", "*/*"},
          {"x-forwarded-proto", "http"},
          {"x-request-id", "6f309897-1bf4-4fe9-8e85-c9690d47d3ee"},
          {"x-envoy-expected-rq-timeout-ms", "15000"},
          {"foo", "bar"},
          {"end-user", "envoy"},
      };
      dealHeadersStatus(filter_->decodeHeaders(request, false));

      // 确认响应报文被插入了cookie
      Http::TestResponseHeaderMapImpl response;
      dealHeadersStatus(filter_->encodeHeaders(response, false));
      EXPECT_TRUE(!Http::Utility::parseSetCookieValue(response, cookie_name).empty());

      // 确认这些请求被路由至不同的上游服务器
      if (upstream_ip.empty()) {
        upstream_ip = getUpstreamAddress();
      } else {
        EXPECT_NE(upstream_ip, getUpstreamAddress());
      }
    }
  }
}

} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
