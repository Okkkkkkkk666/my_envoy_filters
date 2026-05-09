#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_2_x/context.h"
#include "envoy/srhino_plugin_framework/v1_2_x/test/connection_info.h"
#include "envoy/srhino_plugin_framework/v1_2_x/test/header_map.h"
#include "envoy/srhino_plugin_framework/v1_2_x/test/route_info.h"
#include "envoy/srhino_plugin_framework/v1_2_x/test/thread_info.h"
#include "envoy/srhino_plugin_framework/v1_2_x/test/virtual_host_info.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Test {
class MockContext : public virtual Context {
public:
  MockContext() {
    ON_CALL(*this, virtualHostInfo())
        .WillByDefault(
            testing::ReturnRef(dynamic_cast<const SrhinoPluginFramework::v1_2_x::VirtualHostInfo&>(
                virtual_host_info_)));

    ON_CALL(*this, routeInfo())
        .WillByDefault(testing::ReturnRef(
            dynamic_cast<const SrhinoPluginFramework::v1_2_x::RouteInfo&>(route_info_)));

    ON_CALL(*this, connectionInfo())
        .WillByDefault(testing::ReturnRef(
            dynamic_cast<const SrhinoPluginFramework::v1_2_x::ConnectionInfo&>(connection_info_)));

    ON_CALL(*this, threadInfo())
        .WillByDefault(testing::ReturnRef(
            dynamic_cast<const SrhinoPluginFramework::v1_2_x::ThreadInfo&>(thread_info_)));
  }

public:
  MOCK_METHOD(const VirtualHostInfo&, virtualHostInfo, (), (const));
  MOCK_METHOD(const RouteInfo&, routeInfo, (), (const));
  MOCK_METHOD(const ConnectionInfo&, connectionInfo, (), (const));
  MOCK_METHOD(const ThreadInfo&, threadInfo, (), (const));
  MOCK_METHOD(void, setDirectResponse,
              (Utility::HttpCode, std::unique_ptr<HeaderMap>&&, const std::string&), ());
  MOCK_METHOD(void, setDirectResponse,
              (Utility::HttpCode, std::unique_ptr<HeaderMap>&&, std::string&&), ());
  MOCK_METHOD((std::unordered_map<std::string, std::string>&), sharedData, (), ());
  MOCK_METHOD(TimerSharedPtr, createTimer,
              (std::chrono::milliseconds, std::function<bool(Timer& timer)>, bool), ());
  MOCK_METHOD(void, overrideDestHost, (const std::string_view&), ());
  MOCK_METHOD(void, setWaitForBodyBufferLimit, (uint32_t), ());
  MOCK_METHOD(uint32_t, waitForBodyBufferLimit, (), (const));
  MOCK_METHOD(std::unique_ptr<HeaderMap>, createHeaderMap, (), (const));
  MOCK_METHOD(std::unique_ptr<HeaderMap>, createHeaderMap, ((const std::initializer_list<std::pair<std::string_view, std::string_view>>&)), (const));

public:
  testing::NiceMock<SrhinoPluginFramework::v1_2_x::Test::MockVirtualHostInfo> virtual_host_info_;
  testing::NiceMock<SrhinoPluginFramework::v1_2_x::Test::MockRouteInfo> route_info_;
  testing::NiceMock<SrhinoPluginFramework::v1_2_x::Test::MockConnectionInfo> connection_info_;
  testing::NiceMock<SrhinoPluginFramework::v1_2_x::Test::MockThreadInfo> thread_info_;
};

class MockHeaderContext : public MockContext, public HeaderContext {
public:
  MockHeaderContext() {
    ON_CALL(*this, headers()).WillByDefault(testing::ReturnRef(header_map_));
    ON_CALL(testing::Const(*this), headers()).WillByDefault(testing::ReturnRef(header_map_));
  }

public:
  MOCK_METHOD(HeaderMap&, headers, (), ());
  MOCK_METHOD(const HeaderMap&, headers, (), (const));
  MOCK_METHOD(void, setBreakBody, (bool), ());
  MOCK_METHOD(bool, hasBody, (), (const));
  MOCK_METHOD(void, continued, (), (const));

public:
  testing::NiceMock<SrhinoPluginFramework::Test::MockHeaderMap> header_map_;
};

class MockBodyContext : public MockContext, public BodyContext {
public:
  MOCK_METHOD(DataSlices&, data, (), ());
  MOCK_METHOD(bool, hasMoreBody, (), (const));
  MOCK_METHOD(void, continued, (), (const));
};

class MockTrailerContext : public MockContext, public TrailerContext {
public:
  MOCK_METHOD(HeaderMap&, headers, (), ());
  MOCK_METHOD(const HeaderMap&, headers, (), (const));
};
} // namespace Test
} // namespace v1_2_x
} // namespace SrhinoPluginFramework