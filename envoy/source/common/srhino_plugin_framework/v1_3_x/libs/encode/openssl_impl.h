#pragma once

#include "envoy/srhino_plugin_framework/v1_3_x/libs/encode/openssl.h"
#include "openssl/aes.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
namespace Encode {
class OpenSslImpl : public OpenSsl {
public:
  OpenSslImpl();

public:
  class AesImpl : public Aes {
  public:
    std::string encode(const void* data, size_t size, const std::string& key,
                       Mode mode = Mode::CBC) const override;
    std::string decode(const void* data, size_t size, const std::string& key,
                       Mode mode = Mode::CBC) const override;
    void decode(const std::vector<unsigned char>& encrypted_data,
                std::vector<unsigned char>& decrypted_data, const std::vector<uint8_t>& key,
                const std::vector<uint8_t>& iv, Mode mode = Mode::CFB) const override;

  private:
    // 将key的长度修改为128(bits)、192(bits)、256(bits)
    static std::string normalizeKey(const std::string& key);
    // 采用PKCS7标准将待加密数据长度修改为AES_BLOCK_SIZE的整数倍
    static std::string padding(const void* data, size_t size);

    struct State {
      State();
      unsigned char ivec_[AES_BLOCK_SIZE];
      int num_;
      unsigned char ecount_[AES_BLOCK_SIZE];
    };

  private:
    static constexpr size_t bits128_ = 128 / 8;
    static constexpr size_t bits192_ = 192 / 8;
    static constexpr size_t bits256_ = 256 / 8;
  };

  class Md5Impl : public Md5 {};

public:
  AesSharedPtr aes() const override { return aes_; }
  Md5SharedPtr md5() const override { return md5_; }

private:
  AesSharedPtr aes_;
  Md5SharedPtr md5_;
};
} // namespace Encode
} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework