#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_1_x/connection_info.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Test {
class MockConnectionInfo : public ConnectionInfo {
public:
  MockConnectionInfo() {
    ON_CALL(*this, upstreamName).WillByDefault(testing::ReturnRef(upstream_name_));
  }

public:
  MOCK_METHOD(uint32_t, downstreamRemoteAddress, (), (const));
  MOCK_METHOD(uint16_t, downstreamRemotePort, (), (const));
  MOCK_METHOD(uint32_t, downstreamLocalAddress, (), (const));
  MOCK_METHOD(uint16_t, downstreamLocalPort, (), (const));
  MOCK_METHOD(uint32_t, upstreamRemoteAddress, (), (const));
  MOCK_METHOD(uint16_t, upstreamRemotePort, (), (const));
  MOCK_METHOD(uint32_t, upstreamLocalAddress, (), (const));
  MOCK_METHOD(uint16_t, upstreamLocalPort, (), (const));
  MOCK_METHOD(const std::string&, upstreamName, (), (const));
  MOCK_METHOD(Protocol, protocol, (), (const));
  MOCK_METHOD(std::string, sessionId, (), (const));

public:
  std::string upstream_name_{"fake_name"};
};
} // namespace Test
} // namespace v1_1_x
} // namespace SrhinoPluginFramework