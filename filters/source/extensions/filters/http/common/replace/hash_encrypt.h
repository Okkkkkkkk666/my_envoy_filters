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
#include "third-party/Cryptopp/sm3.h"
#include <iostream>
#include <iomanip>
#include <regex>
#include <memory>
#include <string>
#include <algorithm>
#include <sstream>
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
enum HashEncryptionAlgorithm {
  HASH_UNKNOWN = 0,
  HASH_MD5 = 1,
  HASH_SHA256 = 2,
  HASH_SHA512 = 3,
  HASH_SM3 = 4
};

struct HashEncrypt {
  HashEncryptionAlgorithm encryption_algorithm;
  // 加盐值
  std::string salt_value;
};

class HashEncryptRewrite {
public:
  HashEncryptRewrite(const HashEncrypt& hash_encrypt);

public:
  std::string hashEncrypt(const std::string& data) const;
  std::string MD5(const std::string& data, std::string salt_value) const;
  std::string SHA256(const std::string& data, std::string salt_value) const;
  std::string SHA512(const std::string& data, std::string salt_value) const;
  std::string SM3(const std::string& data, std::string salt_value) const;

private:
  const HashEncryptionAlgorithm encryption_algorithm_;
  const std::string salt_value_;
};
} // namespace Replaces
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy