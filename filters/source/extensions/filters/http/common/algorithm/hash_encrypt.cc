#include "hash_encrypt.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Algorithm {

HashEncryptRewrite::HashEncryptRewrite(const v3::HashEncryptRewrite& hash_encrypt)
    : encryption_algorithm_(hash_encrypt.encryption_algorithm()),
      salt_value_(hash_encrypt.salt_value()) {}

std::string HashEncryptRewrite::hashEncrypt(const std::string& data) const {
  if (data.empty()) {
    return data;
  }
  Replaces::rewrite_rule cpp_rule;
  cpp_rule.method = static_cast<Replaces::rewrite_method>(2);
  cpp_rule.hash_encrpyt.encryption_algorithm =
      static_cast<Replaces::HashEncryptionAlgorithm>(encryption_algorithm_);
  cpp_rule.hash_encrpyt.salt_value = salt_value_;
  Replaces::Rewrite rule(cpp_rule);
  return rule.rewrite(data);
}

} // namespace Algorithm
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
