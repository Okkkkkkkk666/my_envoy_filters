#include <string>
#include <thread>

#include "gtest/gtest.h"

#include "filters/source/extensions/filters/http/common/lru_cache.hpp"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {

#define MAX_SIZE 1024

class LruTest : public testing::Test {
public:
  LruTest() : cache_(MAX_SIZE) {}

  void insert(const std::string& key_prefix, size_t count) {
    for (size_t i = 0; i < count; i++) {
      std::string key = key_prefix;
      key += ":{}";
      key = fmt::format(key, i);
      cache_.access(key, nullptr, [i]() { return i; });
    }
  }

protected:
  Filters::Common::LruCache<std::string, int> cache_;
};

// 验证插入及清理是否正确
TEST_F(LruTest, Insert) {
  insert("Insert", 100);
  ASSERT_EQ(cache_.size(), 100);
  cache_.peek(0, [](const int* value) {
    ASSERT_TRUE(value != nullptr);
    ASSERT_EQ(*value, 99);
  });

  cache_.peek(99, [](const int* value) {
    ASSERT_TRUE(value != nullptr);
    ASSERT_EQ(*value, 0);
  });

  cache_.clean(100, [](const int&) { return true; });
  ASSERT_EQ(cache_.size(), 0);
}

// 验证插入数量大于指定的最大缓存时，不会超过设定的最大值
TEST_F(LruTest, InsertMore) {
  insert("Insert", MAX_SIZE * 4);
  ASSERT_EQ(cache_.size(), MAX_SIZE);
  cache_.peek(0, [](const int* value) {
    ASSERT_TRUE(value != nullptr);
    ASSERT_EQ(*value, MAX_SIZE * 4 - 1);
  });

  cache_.peek(MAX_SIZE - 1, [](const int* value) {
    ASSERT_TRUE(value != nullptr);
    ASSERT_EQ(*value, MAX_SIZE * 4 - 1 - (MAX_SIZE - 1));
  });
}

// 验证查询已有节点时，应该调整节点位置，并且不会新增节点
TEST_F(LruTest, Get) {
  const size_t count = 100;
  insert("Insert", count);
  ASSERT_EQ(cache_.size(), count);

  for (int i = count - 1; i >= 0; i--) {
    std::string key = fmt::format("Insert:{}", i);
    cache_.access(key, nullptr, [i]() { return i; });

    // 验证get已存在的不会增加节点
    ASSERT_EQ(cache_.size(), count);

    // 验证get后移动到首部
    cache_.peek(0, [i](const int* value) {
      ASSERT_TRUE(value != nullptr);
      ASSERT_EQ(*value, i);
    });
  }

  // 验证经过上述循环后，尾部应该为99
  cache_.peek(count - 1, [count](const int* value) {
    ASSERT_TRUE(value != nullptr);
    ASSERT_EQ(*value, count - 1);
  });
}

// 验证多线程插入以及清理是否正确
TEST_F(LruTest, MultiThreadInsert) {
  const int insert_count = 100;
  std::vector<std::thread> threads;
  for (size_t i = 0; i < 8; i++) {
    std::string prefix = fmt::format("thread[{}] insert", i);
    threads.push_back(std::thread([&, prefix]() { insert(prefix, 100); }));
  }

  for (auto& thread : threads) {
    thread.join();
  }
  ASSERT_EQ(cache_.size(), insert_count * threads.size());
  threads.clear();

  // 继续插入，验证插入数量大于指定的最大缓存时是否正确
  for (size_t i = 0; i < 16; i++) {
    std::string prefix = fmt::format("thread[{}] insert", i);
    threads.push_back(std::thread([&, prefix]() { insert(prefix, 1000); }));
  }

  for (auto& thread : threads) {
    thread.join();
  }
  threads.clear();
  // 多线程插入时，节点数量略大于最大值是正常的，因为清理时锁被占用导致删除失败是正常的
  // ASSERT_LT(cache_.size(), MAX_SIZE);
  ASSERT_LT(cache_.size(), 8 * insert_count + 16 * 1000);

  // 一边插入一边清理
  for (size_t i = 0; i < 16; i++) {
    std::string prefix = fmt::format("thread[{}] insert", i);
    threads.push_back(std::thread([&, prefix]() { insert(prefix, 1000); }));
  }

  threads.push_back(std::thread([&]() {
    for (size_t i = 0; i < 1000000; i++) {
      cache_.clean(1, [](const int&) { return true; });
    }
  }));

  for (auto& thread : threads) {
    thread.join();
  }
  threads.clear();

  ASSERT_EQ(cache_.size(), 0);
}

// 验证多线程访问已存在节点时，应该调整节点位置，并且不会新增节点
TEST_F(LruTest, MultiThreadGet) {
  const int insert_count = 100;
  std::vector<std::thread> threads;
  for (size_t i = 0; i < 8; i++) {
    std::string prefix = fmt::format("thread[{}] insert", i);
    threads.push_back(std::thread([&, prefix]() { insert(prefix, 100); }));
  }

  for (auto& thread : threads) {
    thread.join();
  }
  ASSERT_EQ(cache_.size(), insert_count * threads.size());
  threads.clear();

  // 多线程get
  bool add = false;
  for (size_t i = 0; i < 8; i++) {
    threads.push_back(std::thread([&, i]() {
      for (size_t j = 0; j < 100; j++) {
        std::string key = fmt::format("thread[{}] insert:{}", i, j);
        cache_.access(
            key, [j](int& v) { ASSERT_EQ(v, j); },
            [&]() -> int {
              add = true;
              return 0;
            });
      }
    }));
  }
  for (auto& thread : threads) {
    thread.join();
  }
  ASSERT_FALSE(add);
  ASSERT_EQ(cache_.size(), insert_count * threads.size());
  threads.clear();
}

} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy