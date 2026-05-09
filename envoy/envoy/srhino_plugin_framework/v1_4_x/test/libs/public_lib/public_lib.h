#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_4_x/libs/public_lib/public_lib.h"
#include "envoy/srhino_plugin_framework/v1_4_x/test/libs/public_lib/cache.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Test {
namespace Libs {
namespace PublicLib {

using namespace SrhinoPluginFramework::Libs::PublicLib;
using testing::_;
using testing::Return;

class MockPublicLib : public PublicLib {
public:
  MockPublicLib() {
    cache_ptr_ = std::make_shared<MockPublicLibCache>();
    ON_CALL(*this, createPublicLibCache()).WillByDefault(Return(cache_ptr_));
  }

public:
  MOCK_METHOD(PublicLibCacheSharedPtr, createPublicLibCache, (), ());

public:
  std::shared_ptr<MockPublicLibCache> cache_ptr_;
};

} // namespace ThreadLocal
} // namespace Libs
} // namespace Test
} // namespace v1_4_x
} // namespace SrhinoPluginFramework