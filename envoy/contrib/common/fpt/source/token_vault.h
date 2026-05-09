#pragma once

#include <string>
#include "absl/container/flat_hash_map.h"
#include <list>
#include <shared_mutex>
namespace Envoy {
namespace Extensions {
namespace Common {
namespace FPT {

struct CacheItem {
  std::string ciphertext;
  std::list<std::string>::iterator lru_it; 
};

class TokenVault{
public:
  explicit TokenVault(size_t max_capacity = 1000000);
  ~TokenVault() = default;
  void storeMapping(const std::string& token, const std::string& ciphertext);
  std::string getPlaintext(const std::string& token);

private:
  size_t max_capacity_;
  absl::flat_hash_map<std::string, CacheItem> cache_map_;
  std::list<std::string> lru_list_;
  std::shared_mutex mtx_;
};

} // namespace FPT
} // namespace Common
} // namespace Extensions
} // namespace Envoy