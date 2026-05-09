#pragma once
#include "cover.h"
#include "encrypt_algorithm.h"
#include "hash_encrypt.h"
#include "replace.h"
#include "shuffle.h"
#include "transform.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
enum rewrite_method {
  EncryptAlgorithmMethod = 1,
  HashEncryptMethod = 2,
  CoverMethod = 3,
  ReplaceMethod = 4,
  TransformMethod = 5,
  ShuffleMethod = 6
};

struct rewrite_rule {
  Replaces::rewrite_method method;
  Replaces::Transform transform;
  Replaces::Shuffle shuffle;
  Replaces::Replace replace;
  Replaces::HashEncrypt hash_encrpyt;
  Replaces::EncryptAlgorithm encrpyt_algorithm;
  Replaces::Cover cover;
};

class Rewrite {
public:
  Rewrite(const rewrite_rule& rewrite);

public:
  std::string rewrite(const std::string& data) const;

private:
  std::unique_ptr<Replaces::EncryptAlgorithmRewrite> encrypt_algorithmPtr_;
  std::unique_ptr<Replaces::HashEncryptRewrite> hash_encryptPtr_;
  std::unique_ptr<Replaces::CoverRewrite> coverPtr_;
  std::unique_ptr<Replaces::ReplaceRewrite> replacePtr_;
  std::unique_ptr<Replaces::ShuffleRewrite> shufflePtr_;
  std::unique_ptr<Replaces::TransformRewrite> transformPtr_;
};
using RewriteRule_Ptr = std::shared_ptr<Rewrite>;
} // namespace Replaces
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy