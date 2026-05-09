#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "source/common/srhino_plugin_framework/v1_1_x/header_map_impl.h"
#include "source/common/srhino_plugin_framework/v1_1_x/data_slices_impl.h"
#include "source/common/srhino_plugin_framework/v1_1_x/libs/net/http_call_impl.h"

#include "source/common/http/message_impl.h"
#include "test/mocks/server/factory_context.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Net {
using namespace Envoy;
  
class HttpCallTest : public testing::Test {
public:
  void SetUp(std::string& cluster_name, std::chrono::milliseconds timeout) {
    mock_cluster_ptr_ = std::make_shared<NiceMock<Upstream::MockThreadLocalCluster>>();
    mock_request_ptr_ = std::make_shared<NiceMock<Http::MockAsyncClientRequest>>(&async_client_);
    header_map_ = std::make_shared<SrhinoPluginFramework::v1_1_x::HeaderMapImpl>();
    data_map_ = std::make_shared<SrhinoPluginFramework::v1_1_x::DataSlicesImpl>();

    ON_CALL(mock_context_, clusterManager()).WillByDefault(ReturnRef(mock_cluster_manager_));
    ON_CALL(mock_cluster_manager_, getThreadLocalCluster(_))
        .WillByDefault(Return(mock_cluster_ptr_.get()));
    ON_CALL(*mock_cluster_ptr_, httpAsyncClient()).WillByDefault(ReturnRef(async_client_));

    client_ = std::make_shared<HttpCallImpl>(mock_context_, std::move(cluster_name), timeout);
  }

  bool mockSend(const std::string& method, const std::string& path) {
    return client_->send(method, path, *header_map_, *data_map_);
  }

public:
  NiceMock<Upstream::MockClusterManager> mock_cluster_manager_;
  NiceMock<Server::Configuration::MockFactoryContext> mock_context_;
  NiceMock<Http::MockAsyncClient> async_client_;

  std::shared_ptr<NiceMock<Http::MockAsyncClientRequest>> mock_request_ptr_;
  std::shared_ptr<NiceMock<Upstream::MockThreadLocalCluster>> mock_cluster_ptr_;
  std::shared_ptr<SrhinoPluginFramework::v1_1_x::HeaderMapImpl> header_map_;
  std::shared_ptr<SrhinoPluginFramework::v1_1_x::DataSlicesImpl> data_map_;
  std::shared_ptr<HttpCallImpl> client_;
};

TEST_F(HttpCallTest, GetNullCluster) {
  std::string cluster_name("fake_cluster");
  SetUp(cluster_name, std::chrono::milliseconds(1000));
  EXPECT_CALL(mock_cluster_manager_, getThreadLocalCluster(_)).WillOnce(Return(nullptr));

  bool result = mockSend("GET", "/fake_path");
  EXPECT_FALSE(result);
}

TEST_F(HttpCallTest, SendSuccess) {
  std::string cluster_name("fake_cluster");
  SetUp(cluster_name, std::chrono::milliseconds(1000));
  EXPECT_CALL(async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        Http::ResponseMessagePtr response(new Http::ResponseMessageImpl(
            Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status", "200"}}}));
        callbacks.onSuccess(*mock_request_ptr_, std::move(response));
        return mock_request_ptr_.get();
      }));

  bool result = mockSend("GET", "/fake_path");
  EXPECT_TRUE(result);
}

TEST_F(HttpCallTest, SendFailure) {
  std::string cluster_name("fake_cluster");
  SetUp(cluster_name, std::chrono::milliseconds(1000));
  EXPECT_CALL(async_client_, send_(_, _, _))
      .WillOnce(Invoke([&](Envoy::Http::RequestMessagePtr&,
                           Envoy::Http::AsyncClient::Callbacks& callbacks,
                           const Envoy::Http::AsyncClient::RequestOptions&) {
        callbacks.onFailure(*mock_request_ptr_, Http::AsyncClient::FailureReason::Reset);
        return mock_request_ptr_.get();
      }));

  bool result = mockSend("GET", "/fake_path");
  EXPECT_TRUE(result);
}

} // namespace Net
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework
