#pragma once

#include <mutex>
#include <shared_mutex>

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Utility {
template <class T> class Singleton {
public:
  static T& instance() {
    std::call_once(once_flag_, [&]() {
      static T obj;
      instance_ = &obj;
    });

    return *instance_;
  }

private:
  static std::once_flag once_flag_;
  static T* instance_;
};

template <class T> std::once_flag Singleton<T>::once_flag_;
template <class T> T* Singleton<T>::instance_ = nullptr;

/**
 * 本模板用于构建单例。
 * 当插件的不同实例的配置容易出现重复，可将配置生成哈希，然后根据hash将配置相关的共享指针存入此单例。
 * 当插件的实例比较多时，使用此模板可显著降低内存占用。
 * 此模板是线程安全的，但存入的共享指针需要使用者自己保证是线程安全的。
 */
template<typename T>
class ReadOnlyStorage {
public:
  ReadOnlyStorage() : node_timeout_(3600), node_update_timeout_(60), clean_interval_(600) {
    store_.clear();
    last_clean_ = time(NULL);
  }
  ReadOnlyStorage(int node_timeout, int node_update_timeout, int clean_interval)
      : node_timeout_(node_timeout), node_update_timeout_(node_update_timeout),
        clean_interval_(clean_interval) {
    store_.clear();
    last_clean_ = time(NULL);
  }

  /**
   * 获取共享指针
   * @param hash
   * @return 指针
   */
  T get(const size_t& hash) {
    time_t now = time(NULL);
    bool update = false;

    T t = nullptr;
    {
      std::shared_lock<std::shared_mutex> lock(mtx_);
      auto it = store_.find(hash);
      if (it != store_.end()) {
        auto& node = it->second;

        if ((now - node.last_use) > node_update_timeout_) {
          update = true;
        }
        t = node.t;
      } else {
        return nullptr;
      }
    }

    if (update) {
      std::unique_lock<std::shared_mutex> lock(mtx_);
      auto it = store_.find(hash);
      if (it != store_.end()) {
        auto& node = it->second;
        if ((now - node.last_use) > node_update_timeout_ && (t == node.t)) {
          node.last_use = now;
        }
      }
    }

    return t;
  }

  /**
   * 存入共享指针
   * @param hash 配置hash值
   * @param t 指针
   */
  void set(const size_t& hash, T t) {
    time_t now = time(NULL);

    std::unique_lock<std::shared_mutex> lock(mtx_);
    auto it = store_.find(hash);
    if (it != store_.end()) {
      auto& node = it->second;
      node.t = t;
      node.last_use = now;
    } else {
      store_[hash] = Node(t, now);
      cnt_++;
    }

    if ((now - last_clean_) > clean_interval_) {
      last_clean_ = now;
      //clean
      for (auto it = store_.begin(); it != store_.end();) {
        auto& node = it->second;
        if ((node.t.use_count() <= 1) && (now - node.last_use) > node_timeout_) {
          auto tmp = it;
          ++it;

          cnt_--;
          store_.erase(tmp);
        } else {
          ++it;
        }
      }
    }
  }

private:
  std::shared_mutex mtx_;
  struct Node {
    T t;
    time_t last_use;
  };
  // 存入指针总数
  int cnt_;
  // 上次清理无效指针时间
  time_t last_clean_;
  // 无效指针超时时间
  const int node_timeout_;
  // 指针更新超时时间
  const int node_update_timeout_;
  // 清理无效指针间隔
  const int clean_interval_;
  std::unordered_map<size_t, Node> store_;
};
} // namespace Utility
} // namespace v1_1_x
} // namespace SrhinoPluginFramework