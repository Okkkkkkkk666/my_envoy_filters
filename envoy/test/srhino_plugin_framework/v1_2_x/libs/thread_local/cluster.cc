#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "source/common/srhino_plugin_framework/v1_2_x/header_map_impl.h"
#include "source/common/srhino_plugin_framework/v1_2_x/data_slices_impl.h"
#include "source/common/srhino_plugin_framework/v1_2_x/libs/thread_local/cluster_impl.h"

#include "source/common/http/message_impl.h"
#include "test/mocks/server/factory_context.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace ThreadLocal {
using namespace Envoy;
using testing::_;
using testing::Return;
using testing::ReturnRef;

class ClusterTest : public testing::Test {
public:
  void setUp(const std::string& cluster_name, bool is_mock_cluster = true) {
    mock_cluster_ptr_ = std::make_shared<NiceMock<Upstream::MockThreadLocalCluster>>();

    ON_CALL(mock_context_, clusterManager()).WillByDefault(ReturnRef(mock_cluster_manager_));
    ON_CALL(mock_cluster_manager_, getThreadLocalCluster(_))
        .WillByDefault(Return(is_mock_cluster ? mock_cluster_ptr_.get() : nullptr));

    cluster_ = std::make_shared<ClusterImpl>(mock_context_, cluster_name);

    mock_host_set_ptr_ = std::make_unique<NiceMock<Upstream::MockHostSet>>();
    mock_host_ptr_ = std::make_shared<NiceMock<Upstream::MockHost>>();
  }

  void mockPeekAnotherHost(bool is_mock = true, const std::string& address = "tcp://127.0.0.1:80") {
    address_ = Network::Utility::resolveUrl(address);

    ON_CALL(*mock_cluster_ptr_, loadBalancer()).WillByDefault(ReturnRef(mock_lb_));
    ON_CALL(mock_lb_, peekAnotherHost(_)).WillByDefault(Return(is_mock ? mock_host_ptr_ : nullptr));
    ON_CALL(*mock_host_ptr_, address()).WillByDefault(Return(address_));
  }

  void mockPrioritySet() {
    ON_CALL(*mock_cluster_ptr_, prioritySet()).WillByDefault(ReturnRef(mock_priority_set_));
    ON_CALL(Const(mock_priority_set_), hostSetsPerPriority()).WillByDefault(ReturnRef(host_sets_));
    ON_CALL(*mock_host_set_ptr_, hosts()).WillByDefault(ReturnRef(host_set_));
    host_sets_.resize(1);
    host_sets_[0] = std::move(mock_host_set_ptr_);
  }


  void setHostSet(size_t size, const std::string& address = "tcp://127.0.0.1:80",
                  bool healthy = true) {
    address_ = Network::Utility::resolveUrl(address);
    for (size_t i = 0; i < size; ++i) {
      auto mock_host = std::make_shared<NiceMock<Upstream::MockHost>>();
      ON_CALL(*mock_host, address()).WillByDefault(Return(address_));
      ON_CALL(*mock_host, health())
          .WillByDefault(Return(healthy ? Upstream::Host::Health::Healthy
                                        : Upstream::Host::Health::Unhealthy));
      host_set_.emplace_back(mock_host);
    }
  }

public:
  NiceMock<Upstream::MockLoadBalancer> mock_lb_;
  NiceMock<Upstream::MockPrioritySet> mock_priority_set_;
  NiceMock<Upstream::MockClusterManager> mock_cluster_manager_;
  NiceMock<Server::Configuration::MockFactoryContext> mock_context_;
  std::shared_ptr<NiceMock<Upstream::MockHost>> mock_host_ptr_;
  std::unique_ptr<NiceMock<Upstream::MockHostSet>> mock_host_set_ptr_;
  std::shared_ptr<NiceMock<Upstream::MockThreadLocalCluster>> mock_cluster_ptr_;

  std::shared_ptr<ClusterImpl> cluster_;
  std::vector<Upstream::HostSharedPtr> host_set_;
  std::vector<Upstream::HostSetPtr> host_sets_;

  Network::Address::InstanceConstSharedPtr address_;
};

TEST_F(ClusterTest, ClusterNullPeekHost) {
  setUp("fake_cluster", false);

  std::string host = cluster_->peekAnotherHost();
  EXPECT_EQ(host, EMPTY_STRING);
}

TEST_F(ClusterTest, PeekHostIsNull) {
  setUp("fake_cluster");
  mockPeekAnotherHost(false);

  std::string host = cluster_->peekAnotherHost();
  EXPECT_EQ(host, EMPTY_STRING);
}

TEST_F(ClusterTest, PeekHostNotNull) {
  setUp("fake_cluster");
  mockPeekAnotherHost(true, "tcp://127.0.0.1:8080");

  std::string host = cluster_->peekAnotherHost();
  EXPECT_EQ(host, "127.0.0.1:8080");
}

TEST_F(ClusterTest, NullEndpoint) {
  setUp("fake_cluster");
  setHostSet(0);
  mockPrioritySet();

  auto endpoints = cluster_->endpointHealthy();

  EXPECT_EQ(endpoints.size(), 0);
}

TEST_F(ClusterTest, EndpointHealthy) {
  setUp("fake_cluster");
  setHostSet(3, "tcp://127.0.0.1:8080");
  mockPrioritySet();

  auto endpoints = cluster_->endpointHealthy();

  EXPECT_EQ(endpoints.size(), 3);
  for (const auto& endpoint : endpoints) {
    EXPECT_EQ(endpoint->address, "127.0.0.1:8080");
    EXPECT_TRUE(endpoint->healthy);
  }
}

TEST_F(ClusterTest, EndpointUnhealthy) {
  setUp("fake_cluster");
  setHostSet(2, "tcp://127.0.0.1:8080", false);
  mockPrioritySet();

  auto endpoints = cluster_->endpointHealthy();

  EXPECT_EQ(endpoints.size(), 2);
  for (const auto& endpoint : endpoints) {
    EXPECT_FALSE(endpoint->healthy);
  }
}

TEST_F(ClusterTest, MixedHealthyUnhealthyEndpoints) {
  setUp("fake_cluster");
  setHostSet(1, "tcp://127.0.0.1:8080", true);
  setHostSet(2, "tcp://127.0.0.1:9090", false);
  mockPrioritySet();

  auto endpoints = cluster_->endpointHealthy();

  EXPECT_EQ(endpoints.size(), 3);
  EXPECT_EQ(endpoints[0]->address, "127.0.0.1:8080");
  EXPECT_EQ(endpoints[1]->address, "127.0.0.1:9090");
  EXPECT_EQ(endpoints[2]->address, "127.0.0.1:9090");
  EXPECT_TRUE(endpoints[0]->healthy);
  EXPECT_FALSE(endpoints[1]->healthy);
  EXPECT_FALSE(endpoints[2]->healthy);
}

} // namespace ThreadLocal
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework