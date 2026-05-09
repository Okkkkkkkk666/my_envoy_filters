#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "filters/source/extensions/filters/http/common/ratelimit/ratelimit_client.h"
#include "filters/source/extensions/filters/http/common/ratelimit/impl/ratelimit_client_impl.h"

#include "test/mocks/grpc/mocks.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/mocks/tracing/mocks.h"
#include "test/test_common/printers.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::Ref;
using testing::Return;

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace RatelimitClient {
namespace Impl {

namespace v3 = envoy::extensions::filters::http::common::ratelimit::v3;

class MockLimitRequestCallbacks : public Filters::Common::RatelimitClient::LimitRequestCallbacks {
public:
  void complete(LimitStatus status, LimitGrpcResponsePtr&& response) override {
    complete_(status, std::move(response));
  }
  MOCK_METHOD(void, complete_, (LimitStatus status, LimitGrpcResponsePtr&& response));
};
class MockCleanRequestCallbacks : public Filters::Common::RatelimitClient::CleanRequestCallbacks {
public:
  void complete(CleanStatus status) override { complete_(status); }
  MOCK_METHOD(void, complete_, (CleanStatus status));
};

class RateLimitGrpcClientTest : public testing::Test {
public:
  RateLimitGrpcClientTest()
      : async_client_(new Grpc::MockAsyncClient()),
        client_(Grpc::RawAsyncClientPtr{async_client_},
                absl::optional<std::chrono::milliseconds>()) {}

  Grpc::MockAsyncClient* async_client_;
  Grpc::MockAsyncRequest async_request_;
  Impl::GrpcClientImpl client_;
  MockLimitRequestCallbacks limit_request_callbacks_;
  MockCleanRequestCallbacks clean_request_callbacks_;
  Tracing::MockSpan span_;
  StreamInfo::MockStreamInfo stream_info_;
};

TEST_F(RateLimitGrpcClientTest, Basic) {
  Quotas quotas = {Quota{1, 1}, Quota{1, 1}};
  std::shared_ptr<RateLimitPolicy> value(new RateLimitPolicy("hello", quotas, true, 5));
  std::vector<std::shared_ptr<RateLimitPolicy>> ratelimit_policys{value};
  {
    v3::RateLimitRequest request;
    auto headers = Http::TestRequestHeaderMapImpl();
    client_.createRequest(request, ratelimit_policys);
    EXPECT_CALL(*async_client_, sendRaw(_, _, Grpc::ProtoBufferEq(request), Ref(client_), _, _))
        .WillOnce(
            Invoke([this](absl::string_view service_full_name, absl::string_view method_name,
                          Buffer::InstancePtr&&, Grpc::RawAsyncRequestCallbacks&, Tracing::Span&,
                          const Http::AsyncClient::RequestOptions&) -> Grpc::AsyncRequest* {
              std::string service_name =
                  "envoy.extensions.filters.http.common.ratelimit.v3.RateLimitService";
              EXPECT_EQ(service_name, service_full_name);
              EXPECT_EQ("ShouldRateLimit", method_name);
              return &async_request_;
            }));
    client_.limit(limit_request_callbacks_, ratelimit_policys, span_, stream_info_);
    client_.onCreateInitialMetadata(headers);
    LimitGrpcResponsePtr message = std::make_unique<v3::RateLimitResponse>();
    message->set_status(v3::RateLimitResponse::OK);
    std::string str;
    message->SerializePartialToString(&str);
    Buffer::InstancePtr response = std::make_unique<Buffer::OwnedImpl>();
    response->add(str);

    EXPECT_CALL(limit_request_callbacks_,
                complete_(Filters::Common::RatelimitClient::LimitStatus::OK, _));
    client_.onSuccessRaw(std::move(response), span_);
  }
}

TEST_F(RateLimitGrpcClientTest, base) {
  std::unique_ptr<v3::RateLimitResponse> response;
  std::vector<std::shared_ptr<Impl::RateLimitPolicy>> ratelimit_policys_{};
  v3::RateLimitRequest request;
  auto headers = Http::TestRequestHeaderMapImpl();
  client_.createRequest(request, ratelimit_policys_);
  EXPECT_CALL(*async_client_, sendRaw(_, _, Grpc::ProtoBufferEq(request), _, _, _))
      .WillOnce(Return(&async_request_));
  client_.limit(limit_request_callbacks_, ratelimit_policys_, span_, stream_info_);
  response = std::make_unique<v3::RateLimitResponse>();
  EXPECT_CALL(limit_request_callbacks_,
              complete_(Filters::Common::RatelimitClient::LimitStatus::Error, _));
  client_.onFailure(Grpc::Status::Unknown, "", span_);
}

TEST_F(RateLimitGrpcClientTest, Cancel) {
  std::vector<std::shared_ptr<Impl::RateLimitPolicy>> ratelimit_policys_{};
  EXPECT_CALL(*async_client_, sendRaw(_, _, _, _, _, _)).WillOnce(Return(&async_request_));
  client_.limit(limit_request_callbacks_, ratelimit_policys_, span_, stream_info_);
  EXPECT_CALL(async_request_, cancel());
  client_.cancel();
}

} // namespace Impl
} // namespace RatelimitClient
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy