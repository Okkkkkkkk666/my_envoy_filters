#pragma once
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include "filters/api/envoy/extensions/filters/http/common/algorithm/v3/algorithm.pb.h"
#include "source/common/common/logger.h"
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
class EncryptAlgorithmRewrite : public Logger::Loggable<Logger::Id::filter>{
public:
  EncryptAlgorithmRewrite(const v3::EncryptAlgorithmRewrite& EncryptAlgorithm);

public:
  std::string encryptAlgorithm(const std::string& data) const;
private:
  const v3::EncryptAlgorithmRewrite_EncryptionAlgorithm encryption_algorithm_;
  const std::string encryption_key_;
};
using EncryptAlgorithmRewritePtr = std::unique_ptr<EncryptAlgorithmRewrite>;
} // namespace Algorithm
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy