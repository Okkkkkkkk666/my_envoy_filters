#include "source/common/srhino_plugin_framework/v1_4_x/libs/encode/compressor_impl.h"
#include "source/common/srhino_plugin_framework/v1_4_x/data_slices_impl.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Encode {
GzipCompressor::GzipCompressor()
    : compressor_([&]() {
        auto compressor =
            std::make_unique<Envoy::Extensions::Compression::Gzip::Compressor::ZlibCompressorImpl>(
                DefaultGzipChunkSize_);
        compressor->init(Envoy::Extensions::Compression::Gzip::Compressor::ZlibCompressorImpl::
                             CompressionLevel::Standard,
                         Envoy::Extensions::Compression::Gzip::Compressor::ZlibCompressorImpl::
                             CompressionStrategy::Standard,
                         (DefaultGzipWindowBits_ | DefaultGzipHeaderValue_),
                         DefaultGzipMemoryLevel_);
        return (compressor);
      }()) {}

void GzipCompressor::compress(DataSlices& buffer) {
  if (compressor_) {
    compressor_->compress(*(dynamic_cast<DataSlicesImpl&>(buffer).raw()),
                          Envoy::Compression::Compressor::State::Finish);
  }
}

BrotliCompressor::BrotliCompressor()
    : compressor_(std::make_unique<
                  Envoy::Extensions::Compression::Brotli::Compressor::BrotliCompressorImpl>(
          DefaultBrotliQuality_, DefaultBrotliWindowBits_, DefaultBrotliInputBlockBits_, false,
          Envoy::Extensions::Compression::Brotli::Compressor::BrotliCompressorImpl::EncoderMode::
              Default,
          DefaultBrotliChunkSize_)) {}

void BrotliCompressor::compress(DataSlices& buffer) {
  if (compressor_) {
    compressor_->compress(*(dynamic_cast<DataSlicesImpl&>(buffer).raw()),
                          Envoy::Compression::Compressor::State::Finish);
  }
}

Encode::DeflateCompressor::DeflateCompressor()
    : compressor_([&]() {
        auto compressor =
            std::make_unique<Envoy::Extensions::Compression::Gzip::Compressor::ZlibCompressorImpl>(
                DefaultGzipChunkSize_);
        compressor->init(Envoy::Extensions::Compression::Gzip::Compressor::ZlibCompressorImpl::
                             CompressionLevel::Standard,
                         Envoy::Extensions::Compression::Gzip::Compressor::ZlibCompressorImpl::
                             CompressionStrategy::Standard,
                         (-DefaultWindowBits_), DefaultGzipMemoryLevel_);
        return (compressor);
      }()) {}

void DeflateCompressor::compress(DataSlices& buffer) {
  if (compressor_) {
    compressor_->compress(*(dynamic_cast<DataSlicesImpl&>(buffer).raw()),
                          Envoy::Compression::Compressor::State::Finish);
  }
}

ZstdCompressor::ZstdCompressor() {}

void ZstdCompressor::compress(DataSlices& buffer) {

  auto& input =
      const_cast<Envoy::Buffer::Instance&>(*(dynamic_cast<const DataSlicesImpl&>(buffer).raw()));
  const uint8_t* data = reinterpret_cast<const uint8_t*>(input.linearize(input.length()));

  // 压缩大小
  size_t compressed_capacity = ZSTD_compressBound(input.length());
  std::vector<uint8_t> compressed_data(compressed_capacity);

  // 压缩
  size_t compressed_size = ZSTD_compress(compressed_data.data(), compressed_capacity, data,
                                         input.length(), ZSTD_CLEVEL_DEFAULT);

  // 清空原有数据并将压缩数据复制回buffer
  buffer.set(
      std::string_view(reinterpret_cast<const char*>(compressed_data.data()), compressed_size));
}

} // namespace Encode
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework
