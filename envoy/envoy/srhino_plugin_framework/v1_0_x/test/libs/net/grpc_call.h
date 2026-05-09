#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_0_x/libs/net/grpc_call.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Test {
namespace Libs {
namespace Net {

using namespace SrhinoPluginFramework::Libs::Net;
using testing::_;
using testing::Return;

class MocGrpcCallback : public GrpcCallback {
public:
  MocGrpcCallback() {}

public:
  MOCK_METHOD(void, complete, (Status, const DataSlices&), ());
};

class MockGrpcCall : public GrpcCall {
public:
  MockGrpcCall() {}

public:
  MOCK_METHOD(void, send, (const std::string&, const DataSlices&, GrpcCallback&), ());
  MOCK_METHOD(void, send, (const std::string&, const void*, size_t, GrpcCallback&), ());
  MOCK_METHOD(void, cancel, (), ());
};

} // namespace Net
} // namespace Libs
} // namespace Test
} // namespace v1_0_x
} // namespace SrhinoPluginFramework