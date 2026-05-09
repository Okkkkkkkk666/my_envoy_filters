#pragma once

#include "envoy/srhino_plugin_framework/v1_1_x/libs/encode/openssl.h"
#include "envoy/srhino_plugin_framework/v1_1_x/libs/encode/decompressor.h"
#include "envoy/srhino_plugin_framework/v1_1_x/libs/encode/compressor.h"
#include "envoy/srhino_plugin_framework/v1_1_x/libs/encode/cryptopp.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Encode {

class Encode {
public:
  virtual ~Encode() = default;

public:
  /**
   * 获取解压缩实例指针
   * @param type 解压缩类型
   * @return 解压缩实例指针
   */
  virtual DecompressorSharedPtr createDecompressor(Decompressor::Type type) = 0;

  /**
   * 获取压缩实例指针
   * @param type 压缩类型
   * @return 压缩实例指针
   */
  virtual CompressorSharedPtr createCompressor(Compressor::Type type) = 0;

  /**
   * 获取OpenSSL实例指针
   * @return OpenSSL实例指针
   */
  virtual OpenSslSharedPtr createOpenSsl() = 0;

  /**
   * 获取Encrypt实例指针
   * @return Encrypt实例指针
   */
  virtual CryptoppSharedPtr createCryptopp() = 0;
};

using EncodeSharedPtr = std::shared_ptr<Encode>;

} // namespace Encode
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework