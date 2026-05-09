#include "source/common/srhino_plugin_framework/v1_4_x/libs/encode/encode_impl.h"
#include "source/common/srhino_plugin_framework/v1_4_x/libs/encode/decompressor_impl.h"
#include "source/common/srhino_plugin_framework/v1_4_x/libs/encode/compressor_impl.h"
#include "source/common/srhino_plugin_framework/v1_4_x/libs/encode/openssl_impl.h"
#include "source/common/srhino_plugin_framework/v1_4_x/libs/encode/cryptopp_impl.h"
#include "encode_impl.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Encode {

DecompressorSharedPtr EncodeImpl::createDecompressor(Decompressor::Type type) {
  if (type == Decompressor::Type::Gzip) {
    return std::make_shared<GzipDecompressor>();
  } else if (type == Decompressor::Type::Brotli) {
    return std::make_shared<BrotliDecompressor>();
  } else if (type == Decompressor::Type::Deflate) {
    return std::make_shared<DeflateDecompressor>();
  } else {
    return std::make_shared<ZstdDecompressor>();
  }
}

CompressorSharedPtr EncodeImpl::createCompressor(Compressor::Type type) {
  if (type == Compressor::Type::Gzip) {
    return std::make_shared<GzipCompressor>();
  } else if (type == Compressor::Type::Brotli) {
    return std::make_shared<BrotliCompressor>();
  } else if (type == Compressor::Type::Deflate) {
    return std::make_shared<DeflateCompressor>();
  } else {
    return std::make_shared<ZstdCompressor>();
  }
}

OpenSslSharedPtr EncodeImpl::createOpenSsl() { return std::make_shared<OpenSslImpl>(); }

CryptoppSharedPtr EncodeImpl::createCryptopp() { return std::make_shared<CryptoppImpl>(); }

} // namespace Encode
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework