#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Encode {
class OpenSsl {
public:
  virtual ~OpenSsl() = default;

public:
  class Aes {
  public:
    virtual ~Aes() = default;

  public:
    enum class Mode {
      ECB, // 电子密码本模式：Electronic code book mode
      CBC, // 密码分组链接：Cipher-block chaining mode
      CFB, // 密文反馈:Cipher feedback mode
      OFB, // 输出反馈：Output feedback mode
      CTR, // 计数器模式 : Counter mode
    };

  public:
    virtual std::string encode(const void* data, size_t size, const std::string& key,
                               Mode mode = Mode::CBC) const = 0;

    std::string encode(const std::string& data, const std::string& key,
                       Mode mode = Mode::CBC) const {
      return encode(data.data(), data.length(), key, mode);
    }

    std::string encode(const std::string_view data, const std::string& key,
                       Mode mode = Mode::CBC) const {
      return encode(data.data(), data.length(), key, mode);
    }

    virtual std::string decode(const void* data, size_t size, const std::string& key,
                               Mode mode = Mode::CBC) const = 0;

    virtual void decode(const std::vector<unsigned char>& encrypted_data,
                        std::vector<unsigned char>& decrypted_data, const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv, Mode mode = Mode::CFB) const = 0;

    std::string decode(const std::string& data, const std::string& key,
                       Mode mode = Mode::CBC) const {
      return decode(data.data(), data.length(), key, mode);
    }

    std::string decode(std::string_view data, const std::string& key, Mode mode = Mode::CBC) const {
      return decode(data.data(), data.length(), key, mode);
    }
  };
  using AesSharedPtr = std::shared_ptr<Aes>;

  class Md5 {
  public:
    virtual ~Md5() = default;

  public:
  };
  using Md5SharedPtr = std::shared_ptr<Md5>;

public:
  virtual AesSharedPtr aes() const = 0;
  virtual Md5SharedPtr md5() const = 0;
};

using OpenSslSharedPtr = std::shared_ptr<OpenSsl>;
} // namespace Encode
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework