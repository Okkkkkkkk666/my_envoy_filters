#pragma once

#include <memory>
#include <string>

#include "envoy/srhino_plugin_framework/v1_3_x/data_slices.h"
namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
namespace Encode {

class Compressor {
public:
  enum class Type { Gzip, Brotli, Deflate, Zstd };

public:
  virtual ~Compressor() = default;

public:
  virtual void compress(DataSlices& buffer) = 0;
};

using CompressorSharedPtr = std::shared_ptr<Compressor>;

} // namespace Encode
} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework