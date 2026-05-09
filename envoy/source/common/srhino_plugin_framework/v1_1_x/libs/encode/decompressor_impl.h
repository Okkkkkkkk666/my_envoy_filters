#pragma once

#include <memory>

#include "envoy/server/factory_context.h"
#include "envoy/srhino_plugin_framework/v1_1_x/libs/encode/decompressor.h"
#include "source/common/stats/isolated_store_impl.h"
#include "source/extensions/compression/brotli/decompressor/brotli_decompressor_impl.h"
#include "source/extensions/compression/gzip/decompressor/zlib_decompressor_impl.h"
#include "third-party/zstd/lib/zstd.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Encode {
class DecompressorImplBase : public Decompressor {
protected:
  static constexpr uint32_t DefaultWindowBits_ = 15;
  static constexpr uint32_t DefaultChunkSize_ = 4096;
  static constexpr uint32_t GzipHeaderValue_ = 16;
}; // namespace Encode

class GzipDecompressor : public DecompressorImplBase {
public:
  GzipDecompressor();

public:
  DataSlicesPtr decompress(const DataSlices& input_buffer) override;

private:
  // 统计指标做黑洞处理
  Envoy::Stats::IsolatedStoreImpl blackhole_scope_;
  const std::unique_ptr<Envoy::Extensions::Compression::Gzip::Decompressor::ZlibDecompressorImpl>
      decompressor_;
};

class BrotliDecompressor : public DecompressorImplBase {
public:
  BrotliDecompressor();

public:
  DataSlicesPtr decompress(const DataSlices& input_buffer) override;

private:
  // 统计指标做黑洞处理
  Envoy::Stats::IsolatedStoreImpl blackhole_scope_;
  const std::unique_ptr<
      Envoy::Extensions::Compression::Brotli::Decompressor::BrotliDecompressorImpl>
      decompressor_;
};

class DeflateDecompressor : public DecompressorImplBase {
public:
  DeflateDecompressor();

public:
  DataSlicesPtr decompress(const DataSlices& input_buffer) override;

private:
  // 统计指标做黑洞处理
  Envoy::Stats::IsolatedStoreImpl blackhole_scope_;
  const std::unique_ptr<Envoy::Extensions::Compression::Gzip::Decompressor::ZlibDecompressorImpl>
      decompressor_;
};


class ZstdDecompressor : public DecompressorImplBase {
public:
  ZstdDecompressor();

public:
  DataSlicesPtr decompress(const DataSlices& input_buffer) override;
};

} // namespace Encode
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework