#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_4_x/route_info.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Test {
class MockRouteInfo : public RouteInfo {
public:
  MockRouteInfo() {
    ON_CALL(*this, name()).WillByDefault(testing::ReturnRef(name_));
    ON_CALL(*this, filterName()).WillByDefault(testing::ReturnRef(name_));
  }

public:
  MOCK_METHOD(const std::string&, name, (), (const));
  MOCK_METHOD(const std::string&, filterName, (), (const));
  MOCK_METHOD(void*, config, (), (const));

public:
  std::string name_{"fake_name"};
};
} // namespace Test
} // namespace v1_4_x
} // namespace SrhinoPluginFramework