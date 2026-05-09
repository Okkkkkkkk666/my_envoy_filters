#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_3_x/libs/thread_local/thread_local.h"
#include "envoy/srhino_plugin_framework/v1_3_x/test/libs/thread_local/cluster.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Test {
namespace Libs {
namespace ThreadLocal {

using namespace SrhinoPluginFramework::Libs::ThreadLocal;
using testing::_;
using testing::Return;

class MockThreadLocal : public ThreadLocal {
public:
  MockThreadLocal() {
    cluster_ptr_ = std::make_shared<MockCluster>();
    ON_CALL(*this, createCluster(_, _)).WillByDefault(Return(cluster_ptr_));
  }

public:
  MOCK_METHOD(ClusterSharedPtr, createCluster, (const std::string&, bool), (const));

public:
  std::shared_ptr<MockCluster> cluster_ptr_;
};

} // namespace ThreadLocal
} // namespace Libs
} // namespace Test
} // namespace v1_3_x
} // namespace SrhinoPluginFramework