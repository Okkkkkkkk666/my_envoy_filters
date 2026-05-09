#pragma once

#include <memory>
#include <string>

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Encode {

class Cryptopp {
public:
  virtual ~Cryptopp() = default;

public:
  enum class Mode {
    ECB, // 电子密码本模式：Electronic code book mode
    CBC, // 密码分组链接：Cipher-block chaining mode
    CFB, // 密文反馈:Cipher feedback mode
    OFB, // 输出反馈：Output feedback mode
    CTR, // 计数器模式 : Counter mode
  };

  enum class RsaMode {
    OAEP_SHA,
    OAEP_SHA256,
    PKCS1v15,
  };

public:
  class Des {
  public:
    virtual ~Des() = default;

  public:
    virtual std::string encode(const std::string& data, const std::string& encryption_key,
                               Mode mode = Mode::CBC) const = 0;
    virtual std::string decode(const std::string& data, const std::string& encryption_key,
                               Mode mode = Mode::CBC) const = 0;
  };
  using DesSharedPtr = std::shared_ptr<Des>;

  class TripleDes {
  public:
    virtual ~TripleDes() = default;

  public:
    virtual std::string encode(const std::string& data, const std::string& encryption_key,
                               Mode mode = Mode::CBC) const = 0;
    virtual std::string decode(const std::string& data, const std::string& encryption_key,
                               Mode mode = Mode::CBC) const = 0;
  };
  using TripleDesSharedPtr = std::shared_ptr<TripleDes>;

  class Aes {
  public:
    virtual ~Aes() = default;

  public:
    virtual std::string encode(const std::string& data, const std::string& encryption_key,
                               Mode mode = Mode::CBC) const = 0;
    virtual std::string decode(const std::string& data, const std::string& encryption_key,
                               Mode mode = Mode::CBC) const = 0;
  };
  using AesSharedPtr = std::shared_ptr<Aes>;

  class Base64 {
  public:
    virtual ~Base64() = default;

  public:
    virtual std::string encode(const std::string& data) const = 0;
    virtual std::string decode(const std::string& data) const = 0;
  };
  using Base64SharedPtr = std::shared_ptr<Base64>;
  class RSA {
  public:
    virtual ~RSA() = default;

  public:
    virtual void generateRSAKeyPair(std::string& private_key, std::string& public_key,
                                    uint32_t key_size) = 0;
    virtual std::string encode(const std::string& data, const std::string& public_key,
                               RsaMode mode = RsaMode::PKCS1v15) const = 0;
    virtual std::string decode(const std::string& data, const std::string& private_key,
                               RsaMode mode = RsaMode::PKCS1v15) const = 0;
  };
  using RSASharedPtr = std::shared_ptr<RSA>;

  class Md5 {
  public:
    virtual ~Md5() = default;

  public:
    virtual std::string encode(const std::string& data) const = 0;
  };
  using Md5SharedPtr = std::shared_ptr<Md5>;

  class Sha256 {
  public:
    virtual ~Sha256() = default;

  public:
    virtual std::string encode(const std::string& data) const = 0;
  };
  using Sha256SharedPtr = std::shared_ptr<Sha256>;

  class Sha512 {
  public:
    virtual ~Sha512() = default;

  public:
    virtual std::string encode(const std::string& data) const = 0;
  };
  using Sha512SharedPtr = std::shared_ptr<Sha512>;

  class Sha1 {
  public:
    virtual ~Sha1() = default;

  public:
    virtual std::string encode(const std::string& data) const = 0;
  };
  using Sha1SharedPtr = std::shared_ptr<Sha1>;

  class Sha3 {
  public:
    virtual ~Sha3() = default;

  public:
    enum class SHA3Type {
      SHA3_224,
      SHA3_256,
      SHA3_384,
      SHA3_512,
    };

  public:
    virtual std::string encode(const std::string& data,
                               SHA3Type type = SHA3Type::SHA3_256) const = 0;
  };
  using Sha3SharedPtr = std::shared_ptr<Sha3>;

  class Sm3 {
  public:
    virtual ~Sm3() = default;

  public:
    virtual std::string encode(const std::string& data) const = 0;
  };
  using Sm3SharedPtr = std::shared_ptr<Sm3>;

public:
  virtual DesSharedPtr des() const = 0;
  virtual TripleDesSharedPtr triple_des() const = 0;
  virtual AesSharedPtr aes() const = 0;
  virtual Base64SharedPtr base64() const = 0;
  virtual RSASharedPtr rsa() const = 0;
  virtual Md5SharedPtr md5() const = 0;
  virtual Sha256SharedPtr sha256() const = 0;
  virtual Sha512SharedPtr Sha512() const = 0;
  virtual Sha1SharedPtr sha1() const = 0;
  virtual Sha3SharedPtr sha3() const = 0;
  virtual Sm3SharedPtr sm3() const = 0;
};
using CryptoppSharedPtr = std::shared_ptr<Cryptopp>;
} // namespace Encode
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework
