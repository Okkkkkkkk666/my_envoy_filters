#include "contrib/common/fpe/fpe.h"

#include <fstream>
#include <mutex>
#include <sstream>

namespace Envoy {
namespace Extensions {
namespace Common {
namespace FPE {

class FPEStub : public FPEInterface {
public:
  FPEStub() = default;
  bool initFromKey(const std::string& key, const std::string& = "") override {
    key_ = key;
    return !key_.empty();
  }

  bool initFromConfigFile(const std::string& path) override {
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    std::string key;
    while (std::getline(in, line)) {
      if (line.find("key:") == 0) {
        key = line.substr(4);
        break;
      }
    }
    key_ = key;
    return !key_.empty();
  }

  // Simple reversible stub: returns "ENC(<hex>)" where <hex> is hex of plaintext XOR key
  std::string encrypt(const std::string& plaintext) override {
    if (key_.empty()) return plaintext;
    std::string out;
    out.reserve(plaintext.size() * 2 + 6);
    out += "ENC(";
    for (size_t i = 0; i < plaintext.size(); ++i) {
      unsigned char c = plaintext[i] ^ key_[i % key_.size()];
      static const char* hex = "0123456789ABCDEF";
      out.push_back(hex[(c >> 4) & 0xF]);
      out.push_back(hex[c & 0xF]);
    }
    out += ")";
    return out;
  }

  std::string decrypt(const std::string& ciphertext) override {
    if (key_.empty()) return ciphertext;
    // expect ENC(...)
    if (ciphertext.size() < 5) return ciphertext;
    if (ciphertext.rfind("ENC(", 0) != 0) return ciphertext;
    if (ciphertext.back() != ')') return ciphertext;
    std::string inner = ciphertext.substr(4, ciphertext.size() - 5);
    std::string out;
    if (inner.size() % 2 != 0) return ciphertext;
    out.reserve(inner.size() / 2);
    for (size_t i = 0; i < inner.size(); i += 2) {
      auto hexval = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
      };
      int hi = hexval(inner[i]);
      int lo = hexval(inner[i + 1]);
      if (hi < 0 || lo < 0) return ciphertext;
      unsigned char c = static_cast<unsigned char>((hi << 4) | lo);
      out.push_back(static_cast<char>(c ^ key_[ (out.size()) % key_.size()]));
    }
    return out;
  }

private:
  std::string key_;
};

FPEInterface& getFPE() {
  static FPEStub instance;
  return instance;
}

} // namespace FPE
} // namespace Common
} // namespace Extensions
} // namespace Envoy
