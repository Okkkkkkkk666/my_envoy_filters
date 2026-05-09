#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_2_x/route_info.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Test {
class MockRouteInfo : public RouteInfo {
public:
  MockRouteInfo() { ON_CALL(*this, name()).WillByDefault(testing::ReturnRef(name_)); }

public:
  MOCK_METHOD(const std::string&, name, (), (const));
  MOCK_METHOD(ConfigPerRouteInterface*, configInternal, (), (const));

protected:
  MOCK_METHOD(std::string, configJsonString, (), (const));
  MOCK_METHOD(void, setConfig, (ConfigPerRouteInterfaceSharedPtr), (const));

public:
  std::string name_{"fake_name"};
};
} // namespace Test
} // namespace v1_2_x
} // namespace SrhinoPluginFramework