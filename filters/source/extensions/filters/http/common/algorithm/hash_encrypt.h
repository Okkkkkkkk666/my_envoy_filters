#pragma once
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include "filters/api/envoy/extensions/filters/http/common/algorithm/v3/algorithm.pb.h"
#include <iostream>
#include <iomanip>
#include <regex>
#include <memory>
#include <string>
#include <algorithm>
#include <sstream>
#include "filters/source/extensions/filters/http/common/replace/rewrite_rule.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Algorithm {

namespace v3 = envoy::extensions::filters::http::common::algorithm::v3;
namespace Replaces = Envoy::Extensions::Filters::Common::Replaces;
class HashEncryptRewrite {
public:
  HashEncryptRewrite(const v3::HashEncryptRewrite& hash_encrypt);

public:
  std::string hashEncrypt(const std::string& data) const;

private:
  const v3::HashEncryptRewrite_HashEncryptionAlgorithm encryption_algorithm_;
  const std::string salt_value_;
};
using HashEncryptRewritePtr = std::unique_ptr<HashEncryptRewrite>;
} // namespace Algorithm
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy