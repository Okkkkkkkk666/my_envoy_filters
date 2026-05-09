#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "source/common/srhino_plugin_framework/v1_0_x/header_map_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/data_slices_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/net/grpc_call_impl.h"

#include "test/mocks/server/factory_context.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace Net {
using namespace Envoy;

class MockGrpcCallback : public GrpcCallback {
public:
  MockGrpcCallback() {}

public:
  MOCK_METHOD(void, complete, (Status status, const DataSlices& response), ());
};

class GrpcCallTest : public testing::Test {
public:
  void setup(const std::string& cluster_name, const std::string& serivce_name) {

    mock_async_client_ptr_ = std::make_shared<NiceMock<Grpc::MockAsyncClient>>();
    mock_request_ptr_ = std::make_shared<NiceMock<Grpc::MockAsyncRequest>>();

    ON_CALL(mock_context_, clusterManager()).WillByDefault(ReturnRef(mock_cluster_manager_));
    ON_CALL(mock_cluster_manager_, grpcAsyncClientManager())
        .WillByDefault(ReturnRef(mock_async_client_manager_));
    ON_CALL(mock_async_client_manager_, getOrCreateRawAsyncClient(_, _, _, _))
        .WillByDefault(Return(mock_async_client_ptr_));
    client_ = std::make_shared<GrpcCallImpl>(mock_context_, cluster_name, serivce_name);
    client_->request_ = mock_request_ptr_.get();
  }
  void mockCallback() { client_->callbacks_ = &mock_cb_; }

public:
  NiceMock<Tracing::MockSpan> span_;
  NiceMock<Server::Configuration::MockFactoryContext> mock_context_;
  NiceMock<Upstream::MockClusterManager> mock_cluster_manager_;
  NiceMock<Grpc::MockAsyncClientManager> mock_async_client_manager_;
  NiceMock<MockGrpcCallback> mock_cb_;

  std::shared_ptr<NiceMock<Grpc::MockAsyncRequest>> mock_request_ptr_;
  std::shared_ptr<NiceMock<Grpc::MockAsyncClient>> mock_async_client_ptr_;
  std::shared_ptr<GrpcCallImpl> client_;
};

TEST_F(GrpcCallTest, SendSuccess) {
  setup("fake_cluster", "fake_server");
  std::string mdthod = "fake_method";
  std::string body = "fake_body";
  EXPECT_CALL(*mock_async_client_ptr_, sendRaw(_, _, _, _, _, _))
      .WillOnce(Invoke([&, this](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                                 Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                                 const Http::AsyncClient::RequestOptions&) {
        std::string data = "buffer";
        auto buffer = GrpcCallImpl::bufferizeData(data.data(), data.size());
        callbacks.onSuccessRaw(std::move(buffer), span_);
        return mock_request_ptr_.get();
      }));
  client_->send(mdthod, body.data(), body.size(), mock_cb_);
}

TEST_F(GrpcCallTest, SendFailure) {
  setup("fake_cluster", "fake_server");
  std::string mdthod = "fake_method";
  std::string body = "fake_body";
  EXPECT_CALL(*mock_async_client_ptr_, sendRaw(_, _, _, _, _, _))
      .WillOnce(Invoke([&, this](absl::string_view, absl::string_view, Buffer::InstancePtr&&,
                                 Grpc::RawAsyncRequestCallbacks& callbacks, Tracing::Span&,
                                 const Http::AsyncClient::RequestOptions&) {
        std::string error = "error";
        callbacks.onFailure(Grpc::Status::WellKnownGrpcStatus::Unavailable, error, span_);
        return nullptr;
      }));
  client_->send(mdthod, body.data(), body.size(), mock_cb_);
}

TEST_F(GrpcCallTest, Cancel) {
  setup("fake_cluster", "fake_server");
  mockCallback();
  EXPECT_CALL(*mock_request_ptr_, cancel).Times(1);
  client_->cancel();
}

} // namespace Net
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework