#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_1_x/thread_info.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Test {
class MockThreadInfo : public ThreadInfo {
public:
  MockThreadInfo() {
    ON_CALL(*this, total_threads)
        .WillByDefault(testing::Return(std::thread::hardware_concurrency()));
    ON_CALL(*this, current_thread_index).WillByDefault(testing::Return(thread_index_));
  }

public:
  MOCK_METHOD(int, total_threads, (), (const));
  MOCK_METHOD(int, current_thread_index, (), (const));

public:
  int thread_index_{0};
};
} // namespace Test
} // namespace v1_1_x
} // namespace SrhinoPluginFramework