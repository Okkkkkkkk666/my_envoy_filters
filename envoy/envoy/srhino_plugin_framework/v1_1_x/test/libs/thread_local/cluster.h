#pragma once

#include <gmock/gmock.h>
#include <srhino_plugin_framework/libs/thread_local/cluster.h>

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Test {
namespace Libs {
namespace ThreadLocal {

using namespace SrhinoPluginFramework::Libs::ThreadLocal;
using testing::_;
using testing::Return;

class MockCluster : public Cluster {
public:
  MockCluster() {}

public:
  MOCK_METHOD(std::string, peekAnotherHost, (), (const));
  MOCK_METHOD(std::vector<std::shared_ptr<EndpointStatus>>, endpointHealthy, (), (const));
};

} // namespace ThreadLocal
} // namespace Libs
} // namespace Test
} // namespace v1_1_x
} // namespace SrhinoPluginFramework