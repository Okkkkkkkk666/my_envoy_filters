#pragma once

#include <memory>
#include <string>

#include "envoy/srhino_plugin_framework/v1_4_x/data_slices.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Encode {

class Decompressor {
public:
  enum class Type { Gzip, Brotli, Deflate, Zstd };

public:
  virtual ~Decompressor() = default;

public:
  virtual DataSlicesPtr decompress(const DataSlices& input_buffer) = 0;
};

using DecompressorSharedPtr = std::shared_ptr<Decompressor>;

} // namespace Encode
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework