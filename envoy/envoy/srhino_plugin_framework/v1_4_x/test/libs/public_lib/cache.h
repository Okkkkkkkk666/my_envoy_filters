#pragma once

#include <gmock/gmock.h>
#include <srhino_plugin_framework/libs/public_lib/cache.h>

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Test {
namespace Libs {
namespace PublicLib {

using namespace SrhinoPluginFramework::Libs::PublicLib;
using testing::_;
using testing::Return;

class MockPublicLibCache : public PublicLibCache {
public:
  MockPublicLibCache() {}

public:
  MOCK_METHOD(std::string, getRule, (const std::string&, const std::string&), ());
};

} // namespace PublicLib
} // namespace Libs
} // namespace Test
} // namespace v1_4_x
} // namespace SrhinoPluginFramework