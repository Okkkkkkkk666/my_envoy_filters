#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_3_x/libs/encode/decompressor.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Test {
namespace Libs {
namespace Encode {
class MockDecompressor : public SrhinoPluginFramework::v1_3_x::Libs::Encode::Decompressor {
public:
  MOCK_METHOD(DataSlicesPtr, decompress, (const DataSlices&), ());
};

} // namespace Encode
} // namespace Libs
} // namespace Test
} // namespace v1_3_x
} // namespace SrhinoPluginFramework