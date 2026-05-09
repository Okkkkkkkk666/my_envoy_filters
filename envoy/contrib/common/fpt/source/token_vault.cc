#include "contrib/common/fpt/source/token_vault.h"
#include <mutex>

namespace Envoy {
namespace Extensions {
namespace Common {
namespace FPT {

void TokenVault::storeMapping(const std::string& token, const std::string& ciphertext) {
  std::unique_lock<std::shared_mutex> lock(mtx_);
  auto it = cache_map_.find(token);
  // token已经存在，cache直接更新，并将token移动到LRU列表头部
  if(it != cache_map_.end()){
    it->second.ciphertext = ciphertext;
    lru_list_.splice(lru_list_.begin(), lru_list_, it->second.lru_it);
  } else {
    // token不存在,插入新映射
    if(cache_map_.size() >= max_capacity_){
      // 超出容量限制，移除LRU列表尾部的token
      const std::string& lru_token = lru_list_.back();
      cache_map_.erase(lru_token);
      lru_list_.pop_back();
    }
    lru_list_.push_front(token);
    cache_map_[token] = {ciphertext, lru_list_.begin()};
  }
}

std::string TokenVault::getPlaintext(const std::string& token) {
  std::unique_lock<std::shared_mutex> lock(mtx_);
  auto it = cache_map_.find(token);
  if(it == cache_map_.end()){
    return ""; // 未找到返回空字符串
  }
  // 将访问的token移动到LRU列表头部
  lru_list_.splice(lru_list_.begin(), lru_list_, it->second.lru_it);
  return it->second.ciphertext;
}

} // namespace FPT
} // namespace Common
} // namespace Extensions
} // namespace Envoy