#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_1_x/libs/encode/compressor.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Test {
namespace Libs {
namespace Encode {
class MockCompressor : public SrhinoPluginFramework::v1_1_x::Libs::Encode::Compressor {
public:
  MOCK_METHOD(void, compress, (DataSlices&), ());
};

} // namespace Encode
} // namespace Libs
} // namespace Test
} // namespace v1_1_x
} // namespace SrhinoPluginFramework