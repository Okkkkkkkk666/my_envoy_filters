#include "encrypt_algorithm.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Algorithm {

EncryptAlgorithmRewrite::EncryptAlgorithmRewrite(
    const v3::EncryptAlgorithmRewrite& EncryptAlgorithm)
    : encryption_algorithm_(EncryptAlgorithm.encryption_algorithm()),
      encryption_key_(EncryptAlgorithm.encryption_key()) {}

std::string EncryptAlgorithmRewrite::encryptAlgorithm(const std::string& data) const {
  if (data.empty()) {
    return data;
  }
  Replaces::rewrite_rule cpp_rule;
  cpp_rule.method = static_cast<Replaces::rewrite_method>(1);
  cpp_rule.encrpyt_algorithm.encryption_algorithm =
      static_cast<Replaces::EncryptionAlgorithm>(encryption_algorithm_);
  cpp_rule.encrpyt_algorithm.encryption_key = encryption_key_;
  Replaces::Rewrite rule(cpp_rule);
  return rule.rewrite(data);
}

} // namespace Algorithm
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
