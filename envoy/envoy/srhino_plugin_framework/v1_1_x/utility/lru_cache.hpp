#pragma once

#include <list>
#include <array>
#include <mutex>
#include <functional>

#include "hash_table.hpp"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Utility {

/**
 * 采用LRU(Least Recently Used)算法进行缓存清理的缓存模板类。
 * 本类的所有方法都是线程安全的。
 */
template <typename KEY, typename VALUE, size_t hash_table_slot_count = 8191> class LruCache {
public:
  LruCache(size_t max_size = 1024 * 16) : max_size_(max_size > 0 ? max_size : 1) {}
  LruCache(const LruCache& cache) = delete;

private:
  struct Node;
  using HashTableType = HashTable<KEY, typename std::list<Node>::iterator, hash_table_slot_count>;

public:
  /**
   *
   * 获取指定key的缓存，并对该节点进行处理。若指定key的缓存不存在时，则添加。
   * 调用此方法后，对应节点移动到缓存首部。
   * @param key 指定的key
   * @param found 找到（添加）指定key的缓存后所做的操作。
   * @param value_factory_cb 当指定key的缓存不存在时，使用该工厂函数生成新的节点。
   */
  void access(const KEY& key, std::function<void(VALUE&)> found,
              std::function<VALUE()> value_factory_cb) {
    map_.readLock(key);

    // 查找节点并处理
    if (lookup(key, found)) {
      map_.readUnlock(key);
      return;
    }

    if (!value_factory_cb) {
      return;
    }

    // 升级为写锁,该操作不是原子的，需要再次验证是否存在
    map_.readUnlock(key);
    map_.writeLock(key);
    if (lookup(key, found)) {
      map_.writeUnlock(key);
      return;
    }

    // 添加节点并处理
    mutex_.lock();
    Node node(value_factory_cb());
    cache_.emplace_front(std::move(node));
    auto cache_iter = cache_.begin();
    cache_iter->table_iter = map_.insert(key, cache_iter);
    if (found) {
      found(cache_iter->value);
    }
    mutex_.unlock();

    map_.writeUnlock(key);

    tryClean(1);
  }

  /**
   * 偷瞄一下指定key的缓存，并对该节点进行处理。
   * 注意：
   * 调用此方法后，并不会对缓存造成任何改动：节点不存在不会增加新节点；节点存在时也不会移动到缓存首部。
   * @param key 指定的key
   * @param found 找到（添加）指定key的缓存后所做的操作。注意，当节点不存在时const VALUE* == nullptr
   */
  void peek(const KEY& key, std::function<void(const VALUE*)> found) {
    if (!found) {
      return;
    }

    map_.readLock(key);

    auto iter = map_.find(key);
    if (iter != map_.end(key)) {
      mutex_.lock();
      found(&(iter->value->value));
      mutex_.unlock();
    } else {
      found(nullptr);
    }

    map_.readUnlock(key);
  }

  /**
   * 偷瞄一下指定位置的缓存，并对该节点进行处理。
   * 注意：
   * 调用此方法后，并不会对缓存造成任何改动：节点不存在不会增加新节点；节点存在时也不会移动到缓存首部。
   * @param pos 指定的位置索引，基于0
   * @param found 找到（添加）指定key的缓存后所做的操作。注意，当节点不存在时const VALUE* == nullptr
   */
  void peek(size_t pos, std::function<void(const VALUE*)> found) {
    if (!found) {
      return;
    }

    mutex_.lock();

    auto iter = cache_.begin();
    for (size_t i = 0; i < pos; i++) {
      ++iter;
    }

    if (iter != cache_.end()) {
      found(&(iter->value));
    } else {
      found(nullptr);
    }

    mutex_.unlock();
  }

  /**
   * 清理缓存
   * @param count
   */
  void clean(size_t count, std::function<bool(const VALUE& value)> predicat) {
    mutex_.lock();

    for (size_t i = 0; i < count; ++i) {
      if (cache_.empty()) {
        break;
      }

      if (!predicat(cache_.back().value)) {
        break;
      }

      const size_t slot_index = cache_.back().table_iter.slotIndex();
      if (!map_.tryWriteLock(slot_index)) {
        break;
      }

      map_.erase(cache_.rbegin()->table_iter);
      cache_.pop_back();
      map_.writeUnlock(slot_index);
    }

    mutex_.unlock();
  }

  /**
   * 获取当前缓存节点的数量
   * @return size_t
   */
  size_t size() {
    std::lock_guard<std::mutex> locker(mutex_);
    return cache_.size();
  }

  size_t maxSize() const { return max_size_; }

private:
  // 缓存的节点类型
  struct Node {
    Node(VALUE&& v) : value(std::forward<VALUE>(v)) {}

    typename HashTableType::iterator table_iter;
    VALUE value;
  };

  std::list<Node> cache_;
  HashTableType map_;
  std::mutex mutex_;
  size_t max_size_;

private:
  // 查找节点并处理
  bool lookup(const KEY& key, std::function<void(VALUE&)> found) {
    auto iter = map_.find(key);
    if (iter != map_.end(key)) {
      mutex_.lock();
      cache_.splice(cache_.begin(), cache_, iter->value);
      if (found) {
        found(iter->value->value);
      }
      mutex_.unlock();

      return true;
    }

    return false;
  }

  // 清理缓存，仅在增加了节点后调用
  void tryClean(size_t count) {
    if (cache_.size() <= max_size_) {
      return;
    }

    mutex_.lock();

    if (cache_.size() > max_size_) {
      for (size_t i = 0; i < count; ++i) {
        const size_t slot_index = cache_.back().table_iter.slotIndex();
        if (!map_.tryWriteLock(slot_index)) {
          break;
        }

        map_.erase(cache_.rbegin()->table_iter);
        cache_.pop_back();
        map_.writeUnlock(slot_index);
      }
    }

    mutex_.unlock();
  }
};

} // namespace Utility
} // namespace v1_1_x
} // namespace SrhinoPluginFramework