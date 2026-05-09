#pragma once
#include <vector>
#include <forward_list>
#include <shared_mutex>
#include <functional>
#include <memory>

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Utility {

/**
 * 本类实现一个简单哈希表，提供一般哈希表的功能外，还提供按槽位上锁功能。
 * 按槽位上锁功能主要应用在多线程同步时，用来减小哈希表的锁粒度，将整个哈希表的一把大锁
 * 拆分成N个小锁，减少锁竞争，提高并发性能。
 * 注意：
 * 本类不支持rehash重排功能，以一个固定质数（8191）作为分组数量，可以根据业务调整
 * 此值，但为了减少哈希碰撞需保证是质数。
 * @tparam KEY
 * @tparam VALUE
 * @tparam slot_count
 */
template <typename KEY, typename VALUE, size_t slot_count = 8191> class HashTable {
public:
  HashTable(std::function<size_t(const KEY&)> hash_func = nullptr,
            size_t per_locker = 8 /*每8个槽位1把锁*/)
      : slots_(slot_count),
        per_locker_(per_locker == 0 ? 1 : (per_locker > slot_count ? slot_count : per_locker)),
        hash_func_(hash_func) {
    size_t locker_count = caclLockerCount(per_locker_);
    lockers_.reserve(locker_count);
    for (size_t i = 0; i < locker_count; i++) {
      lockers_.push_back(std::make_unique<std::shared_mutex>());
    }
  }

public:
  struct Node {
    Node(const KEY& k, const VALUE& v) : key(k), value(v) {}
    KEY key;
    VALUE value;
  };

  // 提供标准库风格的接口
public:
  class iterator {
    friend class HashTable;

  public:
    iterator() : slot_index_(size_t(-1)) {}
    iterator(const iterator& iter) {
      slot_index_ = iter.slot_index_;
      node_iter_ = iter.node_iter_;
    }

  public:
    using list_iterator = typename std::forward_list<Node>::iterator;

    // 重载操作符
  public:
    bool operator==(const iterator& iter) const {
      return slot_index_ == iter.slot_index_ && node_iter_ == iter.node_iter_;
    }
    bool operator!=(const iterator& iter) const { return !(*this == iter); }
    const list_iterator operator->() const { return node_iter_; }
    const list_iterator operator*() const { return node_iter_; }
    list_iterator operator->() { return node_iter_; }
    list_iterator operator*() { return node_iter_; }
    iterator& operator=(const iterator& iter) {
      slot_index_ = iter.slot_index_;
      node_iter_ = iter.node_iter_;
      return *this;
    }
    iterator& operator++() {
      ++node_iter_;
      return *this;
    }

  public:
    size_t slotIndex() const { return slot_index_; }
    list_iterator nodeIter() const { return node_iter_; }

  private:
    size_t slot_index_;
    list_iterator node_iter_;
  };

  /**
   * 查找节点
   * @param key
   * @return iterator
   */
  iterator find(const KEY& key) { return internalFind(getSlotIndex(key), key); }

  /**
   * 插入节点
   * @param key
   * @param value
   * @return iterator
   */
  iterator insert(const KEY& key, const VALUE& value) {
    size_t index = getSlotIndex(key);
    iterator iter = internalFind(index, key);
    if (iter != end(key)) {
      return iter;
    }

    slots_[index].emplace_front(key, value);
    iter.slot_index_ = index;
    iter.node_iter_ = slots_[index].begin();

    return iter;
  }

  /**
   * 插入节点
   * @param key
   * @param value
   * @return iterator
   */
  iterator insert(KEY&& key, VALUE&& value) {
    size_t index = getSlotIndex(key);
    iterator iter = internalFind(index, key);
    if (iter != end(key)) {
      return iter;
    }

    slots_[index].emplace_front(std::forward<KEY>(key), std::forward<VALUE>(value));
    iter.slot_index_ = index;
    iter.node_iter_ = slots_[index].begin();

    return iter;
  }

  /**
   * 删除节点
   * @param iter
   */
  void erase(iterator iter) {
    if (iter.slot_index_ != size_t(-1)) {
      std::forward_list<Node>& list = slots_[iter.slot_index_];
      if (iter.nodeIter() != list.end()) {
        if (list.begin() == iter.nodeIter()) {
          list.pop_front();
        } else {
          for (auto nodeIter = list.begin(); nodeIter != list.end(); ++nodeIter) {
            auto next = nodeIter;
            ++next;
            if (next != list.end() && next == iter.nodeIter()) {
              list.erase_after(nodeIter);
              break;
            }
          }
        }
      }
    }
  }

  /**
   * 获取指定key的起始迭代器
   * @param key
   * @return iterator
   */
  iterator begin(const KEY& key) {
    iterator iter;
    iter.slot_index_ = getSlotIndex(key);
    iter.node_iter_ = slots_[iter.slot_index_].begin();

    return iter;
  }

  /**
   * 获取指定key的结束迭代器
   * @param key
   * @return iterator
   */
  iterator end(const KEY& key) {
    iterator iter;
    iter.slot_index_ = getSlotIndex(key);
    iter.node_iter_ = slots_[iter.slot_index_].end();

    return iter;
  }

  void swap(HashTable& table) {
    slots_.swap(table.slots_);
    lockers_.swap(table.lockers_);
    std::swap(per_locker_, table.per_locker_);
  }

  // 上锁/解锁
public:
  /**
   * 对分组上读锁
   * @param key
   */
  void readLock(const KEY& key) { lockers_[getLockerIndex(getSlotIndex(key))]->lock_shared(); }
  void readLock(size_t index) {
    if (index < slots_.size()) {
      lockers_[getLockerIndex(index)]->lock_shared();
    }
  }
  bool tryReadLock(const KEY& key) {
    return lockers_[getLockerIndex(getSlotIndex(key))]->try_lock_shared();
  }
  bool tryReadLock(size_t index) {
    if (index < slots_.size()) {
      return lockers_[getLockerIndex(index)]->try_lock_shared();
    }

    return false;
  }

  /**
   * 对分组解读锁
   * @param key
   */
  void readUnlock(const KEY& key) { lockers_[getLockerIndex(getSlotIndex(key))]->unlock_shared(); }
  void readUnlock(size_t index) {
    if (index < slots_.size()) {
      lockers_[getLockerIndex(index)]->unlock_shared();
    }
  }

  /**
   * 对分组上写锁
   * @param key
   */
  void writeLock(const KEY& key) { lockers_[getLockerIndex(getSlotIndex(key))]->lock(); }
  void writeLock(size_t index) {
    if (index < slots_.size()) {
      lockers_[getLockerIndex(index)]->lock();
    }
  }
  bool tryWriteLock(const KEY& key) {
    return lockers_[getLockerIndex(getSlotIndex(key))]->try_lock();
  }
  bool tryWriteLock(size_t index) {
    if (index < slots_.size()) {
      return lockers_[getLockerIndex(index)]->try_lock();
    }

    return false;
  }

  /**
   * 对分组解写锁
   * @param key
   */
  void writeUnlock(const KEY& key) { lockers_[getLockerIndex(getSlotIndex(key))]->unlock(); }
  void writeUnlock(size_t index) {
    if (index < slots_.size()) {
      lockers_[getLockerIndex(index)]->unlock();
    }
  }

private:
  std::vector<std::forward_list<Node>> slots_;
  std::vector<std::unique_ptr<std::shared_mutex>> lockers_;
  size_t per_locker_;
  std::function<size_t(const KEY&)> hash_func_;

private:
  /**
   * 根据key获取分组索引。
   * @param key
   * @return size_t
   */
  size_t getSlotIndex(const KEY& key) const {
    if (hash_func_) {
      return hash_func_(key) % slots_.size();
    } else {
      return std::hash<KEY>()(key) % slots_.size();
    }
  }

  /**
   * 计算需要多少把锁
   * @param slot_count 槽位数量
   * @param per_locker  多少个槽位一把锁
   * @return size_t
   */
  size_t caclLockerCount(size_t per_locker) const {
    if (slot_count % per_locker == 0) {
      return slot_count / per_locker;
    } else {
      return (slot_count / per_locker) + 1;
    }
  }

  /**
   * 获取对应槽位的锁索引
   * @param slot_index 槽位索引
   * @return size_t
   */
  size_t getLockerIndex(size_t slot_index) const { return slot_index / per_locker_; }

  /**
   * 查找节点
   * @param slot_index
   * @param key
   * @return iterator
   */
  iterator internalFind(size_t slot_index, const KEY& key) const {
    iterator findIter;
    std::forward_list<Node>& list = const_cast<std::forward_list<Node>&>(slots_[slot_index]);
    findIter.slot_index_ = slot_index;
    findIter.node_iter_ = list.end();
    for (auto iter = list.begin(); iter != list.end(); ++iter) {
      if (iter->key == key) {
        findIter.node_iter_ = iter;
        break;
      }
    }
    return findIter;
  }
};

} // namespace Utility
} // namespace v1_0_x
} // namespace SrhinoPluginFramework