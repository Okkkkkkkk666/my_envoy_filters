#include "hash_encrypt.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
HashEncryptRewrite::HashEncryptRewrite(const HashEncrypt& hash_encrypt)
    : encryption_algorithm_(hash_encrypt.encryption_algorithm),
      salt_value_(hash_encrypt.salt_value) {}

std::string HashEncryptRewrite::hashEncrypt(const std::string& data) const{
  std::string result;
  switch (encryption_algorithm_) {
  case HashEncryptionAlgorithm::HASH_MD5:
    result = MD5(data, salt_value_);
    break;
  case HashEncryptionAlgorithm::HASH_SHA256:
    result = SHA256(data, salt_value_);
    break;
  case HashEncryptionAlgorithm::HASH_SHA512:
    result = SHA512(data, salt_value_);
    break;
  case HashEncryptionAlgorithm::HASH_SM3:
    result = SM3(data, salt_value_);
  default:
    // 处理未知的变换类型
    break;
  }

  return result;
}

std::string HashEncryptRewrite::MD5(const std::string& data, std::string salt_value) const{
  CryptoPP::Weak1::MD5 md5;
  // 加盐值与源数据结合
  auto new_data = data + salt_value;
  CryptoPP::byte digest[CryptoPP::Weak1::MD5::DIGESTSIZE];
  md5.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(new_data.c_str()),
                      new_data.length());

  std::stringstream ss;
  for (int i = 0; i < CryptoPP::Weak1::MD5::DIGESTSIZE; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
  }
  std::string hash = ss.str();
  return hash;
}

std::string HashEncryptRewrite::SHA256(const std::string& data, std::string salt_value) const{
  CryptoPP::SHA256 sha256;
  auto new_data = data + salt_value;
  CryptoPP::byte digest[CryptoPP::SHA256::DIGESTSIZE];
  sha256.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(new_data.c_str()),
                         new_data.length());
  std::stringstream ss;
  for (int i = 0; i < CryptoPP::SHA256::DIGESTSIZE; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
  }
  std::string hash = ss.str();
  return hash;
}

std::string HashEncryptRewrite::SHA512(const std::string& data, std::string salt_value) const{
  CryptoPP::SHA512 sha512;
  auto new_data = data + salt_value;
  CryptoPP::byte digest[CryptoPP::SHA512::DIGESTSIZE];
  sha512.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(new_data.c_str()),
                         new_data.length());
  std::stringstream ss;
  for (int i = 0; i < CryptoPP::SHA512::DIGESTSIZE; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
  }
  std::string hash = ss.str();
  return hash;
}

std::string HashEncryptRewrite::SM3(const std::string& data, std::string salt_value) const{
  CryptoPP::SM3 sm3;
  auto new_data = data + salt_value;
  CryptoPP::byte digest[CryptoPP::SM3::DIGESTSIZE];
  sm3.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(new_data.c_str()),
                      new_data.length());
  std::stringstream ss;
  for (int i = 0; i < CryptoPP::SM3::DIGESTSIZE; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
  }
  std::string hash = ss.str();
  return hash;
}
} // namespace Replaces
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy