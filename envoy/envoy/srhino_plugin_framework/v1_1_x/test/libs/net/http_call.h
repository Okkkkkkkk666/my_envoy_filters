#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_1_x/libs/net/http_call.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Test {
namespace Libs {
namespace Net {

using namespace SrhinoPluginFramework::Libs::Net;
using testing::_;
using testing::Return;

class MockHttpCall : public HttpCall {
public:
  MockHttpCall() {}

public:
  MOCK_METHOD(bool, get, (const std::string_view&, const HeaderMap&, const DataSlices&, Callback),
              ());
  MOCK_METHOD(bool, get, (const std::string_view&, const HeaderMap&, Callback), ());
  MOCK_METHOD(bool, get,
              (const std::string_view&,
               (const std::initializer_list<std::pair<std::string_view, std::string_view>>&),
               const std::string_view&, Callback),
              ());
  MOCK_METHOD(bool, post, (const std::string_view&, const HeaderMap&, const DataSlices&, Callback),
              ());
  MOCK_METHOD(bool, post, (const std::string_view&, const HeaderMap&, Callback), ());
  MOCK_METHOD(bool, post,
              (const std::string_view&,
               (const std::initializer_list<std::pair<std::string_view, std::string_view>>&),
               const std::string_view&, Callback),
              ());
  MOCK_METHOD(void, cancel, (), ());
};

} // namespace Net
} // namespace Libs
} // namespace Test
} // namespace v1_1_x
} // namespace SrhinoPluginFramework