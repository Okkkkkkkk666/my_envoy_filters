#include "rewrite_rule.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
Rewrite::Rewrite(const rewrite_rule& rewrite) {
  switch (rewrite.method) {
  case rewrite_method::EncryptAlgorithmMethod:
    encrypt_algorithmPtr_ = std::make_unique<Replaces::EncryptAlgorithmRewrite>(rewrite.encrpyt_algorithm);
    break;
  case rewrite_method::HashEncryptMethod:
    hash_encryptPtr_ = std::make_unique<Replaces::HashEncryptRewrite>(rewrite.hash_encrpyt);
    break;
  case rewrite_method::ShuffleMethod:
    shufflePtr_ = std::make_unique<Replaces::ShuffleRewrite>(rewrite.shuffle);
    break;
  case rewrite_method::CoverMethod:
    coverPtr_ = std::make_unique<Replaces::CoverRewrite>(rewrite.cover);
    break;
  case rewrite_method::ReplaceMethod:
    replacePtr_ = std::make_unique<Replaces::ReplaceRewrite>(rewrite.replace);
    break;
  case rewrite_method::TransformMethod:
    transformPtr_ = std::make_unique<Replaces::TransformRewrite>(rewrite.transform);
    break;
  default:
    break;
  }
}

std::string Rewrite::rewrite(const std::string& data) const{
  std::string result;
  if (transformPtr_) {
    result = transformPtr_->transForm(data);
    return result;
  }
  if (hash_encryptPtr_) {
    result = hash_encryptPtr_->hashEncrypt(data);
    return result;
  }
  if (encrypt_algorithmPtr_) {
    result = encrypt_algorithmPtr_->encryptAlgorithm(data);
    return result;
  }
  if (coverPtr_) {
    result = coverPtr_->coverData(data);
    return result;
  }
  if (replacePtr_) {
    result = replacePtr_->execudata(data);
    return result;
  }
  result = shufflePtr_->shuffle(data);
  return result;
}
} // namespace Replace
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy