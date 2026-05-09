#pragma once

#include <memory>

#include "envoy/server/factory_context.h"
#include "envoy/srhino_plugin_framework/v1_2_x/libs/encode/compressor.h"
#include "source/extensions/compression/brotli/compressor/brotli_compressor_impl.h"
#include "source/extensions/compression/gzip/compressor/zlib_compressor_impl.h"
#include "third-party/zstd/lib/zstd.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace Encode {

class CompressorImplBase : public Compressor {
protected:
  // Gzip
  static constexpr uint64_t DefaultGzipMemoryLevel_ = 5;
  static constexpr uint64_t DefaultGzipWindowBits_ = 12;
  static constexpr uint64_t DefaultGzipHeaderValue_ = 16;
  static constexpr uint32_t DefaultGzipChunkSize_ = 4096;
  // Brotli
  static constexpr uint32_t DefaultBrotliInputBlockBits_ = 24;
  static constexpr uint32_t DefaultBrotliWindowBits_ = 18;
  static constexpr uint32_t DefaultBrotliQuality_ = 3;
  static constexpr uint32_t DefaultBrotliChunkSize_ = 4096;
  // Deflate
  static constexpr uint64_t DefaultWindowBits_ = 15;
}; // namespace Encode

class GzipCompressor : public CompressorImplBase {
public:
  GzipCompressor();

public:
  void compress(DataSlices& buffer) override;

private:
  const std::unique_ptr<Envoy::Extensions::Compression::Gzip::Compressor::ZlibCompressorImpl>
      compressor_;
};

class BrotliCompressor : public CompressorImplBase {
public:
  BrotliCompressor();

public:
  void compress(DataSlices& buffer) override;

private:
  const std::unique_ptr<Envoy::Extensions::Compression::Brotli::Compressor::BrotliCompressorImpl>
      compressor_;
};

class DeflateCompressor : public CompressorImplBase {
public:
  DeflateCompressor();

public:
  void compress(DataSlices& buffer) override;

private:
  const std::unique_ptr<Envoy::Extensions::Compression::Gzip::Compressor::ZlibCompressorImpl>
      compressor_;
};

class ZstdCompressor : public CompressorImplBase {
public:
  ZstdCompressor();

public:
  void compress(DataSlices& buffer) override;
};

} // namespace Encode
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework