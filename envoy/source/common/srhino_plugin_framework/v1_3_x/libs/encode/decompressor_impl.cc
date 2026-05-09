#include "source/common/srhino_plugin_framework/v1_3_x/libs/encode/decompressor_impl.h"
#include "source/common/srhino_plugin_framework/v1_3_x/data_slices_impl.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
namespace Encode {

GzipDecompressor::GzipDecompressor()
    : decompressor_([&]() {
        auto decompressor = std::make_unique<
            Envoy::Extensions::Compression::Gzip::Decompressor::ZlibDecompressorImpl>(
            blackhole_scope_, "", DefaultChunkSize_);
        decompressor->init(DefaultWindowBits_ | GzipHeaderValue_);
        return (decompressor);
      }()) {}

DataSlicesPtr GzipDecompressor::decompress(const DataSlices& input_buffer) {
  if (decompressor_) {
    DataSlicesPtr output_buffer = input_buffer.create();
    decompressor_->decompress(*(dynamic_cast<const DataSlicesImpl&>(input_buffer).raw()),
                              *(dynamic_cast<DataSlicesImpl*>(output_buffer.get())->raw()));
    return output_buffer;
  }
  return nullptr;
}

BrotliDecompressor::BrotliDecompressor()
    : decompressor_(std::make_unique<
                    Envoy::Extensions::Compression::Brotli::Decompressor::BrotliDecompressorImpl>(
          blackhole_scope_, "", DefaultChunkSize_, false)) {}

DataSlicesPtr BrotliDecompressor::decompress(const DataSlices& input_buffer) {
  if (decompressor_) {
    DataSlicesPtr output_buffer = input_buffer.create();
    decompressor_->decompress(*(dynamic_cast<const DataSlicesImpl&>(input_buffer).raw()),
                              *(dynamic_cast<DataSlicesImpl*>(output_buffer.get())->raw()));
    return output_buffer;
  }
  return nullptr;
}

Encode::DeflateDecompressor::DeflateDecompressor()
    : decompressor_([&]() {
        auto decompressor = std::make_unique<
            Envoy::Extensions::Compression::Gzip::Decompressor::ZlibDecompressorImpl>(
            blackhole_scope_, "", DefaultChunkSize_);
        decompressor->init(-DefaultWindowBits_);
        return (decompressor);
      }()) {}

DataSlicesPtr DeflateDecompressor::decompress(const DataSlices& input_buffer) {
  if (decompressor_) {
    DataSlicesPtr output_buffer = input_buffer.create();
    decompressor_->decompress(*(dynamic_cast<const DataSlicesImpl&>(input_buffer).raw()),
                              *(dynamic_cast<DataSlicesImpl*>(output_buffer.get())->raw()));
    return output_buffer;
  }

  return nullptr;
}

ZstdDecompressor::ZstdDecompressor() {}

DataSlicesPtr ZstdDecompressor::decompress(const DataSlices& input_buffer) {
  // 获取输入数据
  auto& input = const_cast<Envoy::Buffer::Instance&>(
      *(dynamic_cast<const DataSlicesImpl&>(input_buffer).raw()));
  const uint8_t* data = reinterpret_cast<const uint8_t*>(input.linearize(input.length()));

  // 解压缩后的大小
  size_t decompressed_size = ZSTD_getFrameContentSize(data, input.length());
  if (decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
    return nullptr;
  } else if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
    decompressed_size = input.length();
  }

  // 分配解压缩缓冲区
  std::vector<uint8_t> decompressed_data(decompressed_size);

  // 解压缩数据
  size_t actual_decompressed_size =
      ZSTD_decompress(decompressed_data.data(), decompressed_size, data, input.length());
  if (ZSTD_isError(actual_decompressed_size)) {
    return nullptr;
  }

  DataSlicesPtr output_buffer = input_buffer.create();
  output_buffer->set(std::string_view(reinterpret_cast<const char*>(decompressed_data.data()),
                                      actual_decompressed_size));
  return output_buffer;
}

} // namespace Encode
} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework
