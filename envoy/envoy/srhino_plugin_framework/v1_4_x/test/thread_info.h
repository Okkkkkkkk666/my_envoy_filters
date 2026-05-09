#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_4_x/thread_info.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Test {
class MockThreadInfo : public ThreadInfo {
public:
  MockThreadInfo() {
    ON_CALL(*this, totalThreads)
        .WillByDefault(testing::Return(std::thread::hardware_concurrency()));
    ON_CALL(*this, currentThreadIndex).WillByDefault(testing::Return(thread_index_));
  }

public:
  MOCK_METHOD(int, totalThreads, (), (const));
  MOCK_METHOD(int, currentThreadIndex, (), (const));

public:
  int thread_index_{0};
};
} // namespace Test
} // namespace v1_4_x
} // namespace SrhinoPluginFramework