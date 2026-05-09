#include <set>

#include "gtest/gtest.h"

#include "filters/source/extensions/filters/http/common/hash_table.hpp"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {

class HashTableTest : public testing::Test {
public:
  HashTableTest() = default;
  void insert() {
    for (size_t i = 0; i < slot_count_; i++) {
      table_.insert(i, i);
    }
  }

  void insertMore() {
    for (size_t i = 0; i < slot_count_ * same_count_; i++) {
      table_.insert(i, i);
    }
  }

protected:
  size_t slot_count_{8191};
  size_t same_count_{4};
  Filters::Common::HashTable<int, int, 8191> table_;
};

// 验证表为空时的查找以及遍历迭代器是否正常
TEST_F(HashTableTest, EmptyIter) {
  for (size_t i = 0; i < slot_count_; i++) {
    auto iter = table_.find(i);
    ASSERT_TRUE(iter == table_.end(i));
    ASSERT_TRUE(table_.begin(i) == table_.end(i));
  }
}

// 验证插入及查找是否正确
TEST_F(HashTableTest, InsertAndFind) {
  insert();

  for (size_t i = 0; i < slot_count_; i++) {
    auto iter = table_.find(i);
    ASSERT_TRUE(iter != table_.end(i));
    ASSERT_TRUE(iter.slotIndex() == i);
    ASSERT_TRUE(iter->value == static_cast<int>(i));
  }
}

// 验证删除是否正确
TEST_F(HashTableTest, Erase) {
  insert();

  for (size_t i = 0; i < slot_count_; i++) {
    auto iter = table_.find(i);
    ASSERT_TRUE(iter != table_.end(i));
    table_.erase(iter);
  }

  for (size_t i = 0; i < slot_count_; i++) {
    auto iter = table_.find(i);
    ASSERT_TRUE(iter == table_.end(i));
  }
}

// 验证插入时的“拉链法”是否正确
TEST_F(HashTableTest, InsertMore) {
  insertMore();

  for (size_t i = 0; i < slot_count_ * same_count_; i++) {
    auto iter = table_.find(i);
    ASSERT_TRUE(iter != table_.end(i));
  }

  struct Node {
    Node(int k, int v) : key(k), value(v) {}
    int key;
    int value;
  };

  for (size_t i = 0; i < slot_count_ * same_count_; i++) {
    std::vector<Node> nodes;
    for (auto iter = table_.begin(i); iter != table_.end(i); ++iter) {
      nodes.push_back(Node(iter->key, iter->value));
    }

    ASSERT_EQ(nodes.size(), same_count_);
    ASSERT_EQ(nodes[0].key, nodes[0].value);
    ASSERT_EQ(nodes[1].key, nodes[1].value);
    ASSERT_EQ(nodes[2].key, nodes[2].value);
    ASSERT_EQ(nodes[3].key, nodes[3].value);
    ASSERT_EQ(nodes[0].key - slot_count_, nodes[1].key);
    ASSERT_EQ(nodes[1].key - slot_count_, nodes[2].key);
    ASSERT_EQ(nodes[2].key - slot_count_, nodes[3].key);
  }
}

// 验证随机删除是否正确
TEST_F(HashTableTest, RandomErase) {
  insertMore();

  constexpr size_t same_count = 4;
  std::set<int> remove_set;

  srand(time(nullptr));
  for (size_t i = 0; i < slot_count_ * same_count; i++) {
    int key = 0;
    do {
      key = rand() % (slot_count_ * same_count);
    } while (remove_set.find(key) != remove_set.end());
    remove_set.insert(key);

    auto iter = table_.find(key);
    ASSERT_TRUE(iter != table_.end(key));
    table_.erase(iter);
    iter = table_.find(key);
    ASSERT_TRUE(iter == table_.end(key));
  }

  ASSERT_EQ(remove_set.size(), slot_count_ * same_count);
}

} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy