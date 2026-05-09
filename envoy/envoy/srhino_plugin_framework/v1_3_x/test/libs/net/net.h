#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_3_x/libs/net/net.h"
#include "envoy/srhino_plugin_framework/v1_3_x/test/libs/net/grpc_call.h"
#include "envoy/srhino_plugin_framework/v1_3_x/test/libs/net/http_call.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Test {
namespace Libs {
namespace Net {

using namespace SrhinoPluginFramework::Libs::Net;
using testing::_;
using testing::Return;

class MockNet : public Net {
public:
  MockNet() {
    http_call_ptr_ = std::make_shared<MockHttpCall>();
    grpc_call_ptr_ = std::make_shared<MockGrpcCall>();
    ON_CALL(*this, createHttpCall(_, _, _)).WillByDefault(Return(http_call_ptr_));
    ON_CALL(*this, createGrpcCall(_, _, _)).WillByDefault(Return(grpc_call_ptr_));
  }

public:
  MOCK_METHOD(HttpCallSharedPtr, createHttpCall,
              (const std::string&, const std::chrono::milliseconds&, bool), ());
  MOCK_METHOD(GrpcCallSharedPtr, createGrpcCall, (const std::string&, const std::string&, bool),
              ());

public:
  std::shared_ptr<MockHttpCall> http_call_ptr_;
  std::shared_ptr<MockGrpcCall> grpc_call_ptr_;
};

} // namespace Net
} // namespace Libs
} // namespace Test
} // namespace v1_3_x
} // namespace SrhinoPluginFramework