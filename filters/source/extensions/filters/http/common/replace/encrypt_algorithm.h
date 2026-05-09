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
enum EncryptionAlgorithm { ENCRYPTION_UNKNOWN = 0, DES = 1, TRIPLE_DES = 2, AES = 3 };
struct EncryptAlgorithm {
  EncryptionAlgorithm encryption_algorithm;
  // 密钥
  std::string encryption_key;
};

class EncryptAlgorithmRewrite {
public:
  EncryptAlgorithmRewrite(const EncryptAlgorithm& EncryptAlgorithm);

public:
  std::string encryptAlgorithm(const std::string& data) const;
  std::string DES(const std::string& datam, const std::string& encryption_key) const;
  std::string TRIPLE_DES(const std::string& data, const std::string& encryption_key) const;
  std::string AES(const std::string& data, const std::string& encryption_key) const;
  std::string encryptionKey() const { return encryption_key_; }

private:
  const EncryptionAlgorithm encryption_algorithm_;
  const std::string encryption_key_;
};
} // namespace Replaces
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy