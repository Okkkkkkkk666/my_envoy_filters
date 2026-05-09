#pragma once
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1

#include "third-party/Cryptopp/aes.h"
#include "third-party/Cryptopp/des.h"
#include "third-party/Cryptopp/base64.h"
#include "third-party/Cryptopp/modes.h"
#include "third-party/Cryptopp/filters.h"
#include "third-party/Cryptopp/cryptlib.h"
#include "third-party/Cryptopp/hex.h"
#include "third-party/Cryptopp/filters.h"
#include "third-party/Cryptopp/md5.h"
#include "third-party/Cryptopp/sha.h"
#include "third-party/Cryptopp/sha3.h"
#include "third-party/Cryptopp/sm3.h"
#include "third-party/Cryptopp/rsa.h"
#include "third-party/Cryptopp/osrng.h"
#include "third-party/Cryptopp/secblock.h"
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <algorithm>
#include <sstream>

#include "envoy/srhino_plugin_framework/v1_2_x/libs/encode/cryptopp.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace Encode {

class CryptoppImpl : public Cryptopp {
public:
  CryptoppImpl();

public:
  // 采用PKCS7标准将待加密数据长度修改为AES_BLOCK_SIZE的整数倍
  static std::string padData(const std::string& data, size_t block_size);

public:
  class DesImpl : public Des {
  public:
    std::string encode(const std::string& data, const std::string& encryption_key,
                       Mode mode) const override;
    std::string decode(const std::string& data, const std::string& encryption_key,
                       Mode mode) const override;

  private:
    // 将key的长度修改为64(bits)
    static std::string normalizeKey(const std::string& key);

  private:
    static constexpr size_t bits64_ = 64 / 8;
  };

public:
  class TripleDesImpl : public TripleDes {
  public:
    std::string encode(const std::string& data, const std::string& encryption_key,
                       Mode mode) const override;
    std::string decode(const std::string& data, const std::string& encryption_key,
                       Mode mode) const override;

  private:
    // 将key的长度修改为192(bits)、256(bits)
    static std::string normalizeKey(const std::string& key);

  private:
    static constexpr size_t bits192_ = 192 / 8;
    static constexpr size_t bits256_ = 256 / 8;
  };

public:
  class AesImpl : public Aes {
  public:
    std::string encode(const std::string& data, const std::string& encryption_key,
                       Mode mode) const override;
    std::string decode(const std::string& data, const std::string& encryption_key,
                       Mode mode) const override;

  private:
    // 将key的长度修改为128(bits)、192(bits)、256(bits)
    static std::string normalizeKey(const std::string& key);

  private:
    static constexpr size_t bits128_ = 128 / 8;
    static constexpr size_t bits192_ = 192 / 8;
    static constexpr size_t bits256_ = 256 / 8;
  };

public:
  class Base64Impl : public Base64 {
  public:
    std::string encode(const std::string& data) const override;
    std::string decode(const std::string& data) const override;
  };

public:
  class RSAImpl : public RSA {
  public:
    void generateRSAKeyPair(std::string& private_key, std::string& public_key,
                            uint32_t key_size) override;
    std::string encode(const std::string& data, const std::string& public_key,
                       RsaMode mode) const override;
    std::string decode(const std::string& data, const std::string& private_key,
                       RsaMode mode) const override;
  };

public:
  class Md5Impl : public Md5 {
  public:
    std::string encode(const std::string& data) const override;
  };

public:
  class Sha256Impl : public Sha256 {
  public:
    std::string encode(const std::string& data) const override;
  };

public:
  class Sha512Impl : public Sha512 {
  public:
    std::string encode(const std::string& data) const override;
  };

public:
  class Sha1Impl : public Sha1 {
  public:
    std::string encode(const std::string& data) const override;
  };

public:
  class Sha3Impl : public Sha3 {
  public:
    std::string encode(const std::string& data, SHA3Type type = SHA3Type::SHA3_256) const override;
  };

public:
  class Sm3Impl : public Sm3 {
  public:
    std::string encode(const std::string& data) const override;
  };

public:
  DesSharedPtr des() const override { return std::make_shared<CryptoppImpl::DesImpl>(); }
  TripleDesSharedPtr triple_des() const override { return std::make_shared<CryptoppImpl::TripleDesImpl>(); }
  AesSharedPtr aes() const override { return std::make_shared<CryptoppImpl::AesImpl>(); }
  Base64SharedPtr base64() const override { return std::make_shared<CryptoppImpl::Base64Impl>(); }
  RSASharedPtr rsa() const override { return std::make_shared<CryptoppImpl::RSAImpl>(); }
  Md5SharedPtr md5() const override { return std::make_shared<CryptoppImpl::Md5Impl>(); }
  Sha256SharedPtr sha256() const override { return std::make_shared<CryptoppImpl::Sha256Impl>(); }
  Sha512SharedPtr Sha512() const override { return std::make_shared<CryptoppImpl::Sha512Impl>(); }
  Sha1SharedPtr sha1() const override { return std::make_shared<CryptoppImpl::Sha1Impl>(); }
  Sha3SharedPtr sha3() const override { return std::make_shared<CryptoppImpl::Sha3Impl>(); }
  Sm3SharedPtr sm3() const override { return std::make_shared<CryptoppImpl::Sm3Impl>(); }

};

} // namespace Encode
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework