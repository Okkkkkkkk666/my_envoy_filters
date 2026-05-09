#pragma once

#include <string>

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Utility {
class Hash {
public:
  /**
   * 更新哈希
   * @param data 需要计算哈希的数据
   * @param size_t data长度
   * @param hash_code 接收哈希值。如果该值不为0，则哈希值是增量的。
   */
  static void update(const void* data, size_t len, uint64_t& hash_code) {
    const unsigned char* data_byte = reinterpret_cast<const unsigned char*>(data);
    for (size_t i = 0; i < len; ++i) {
      hash_code = hash_code * 31 + data_byte[i];
    }
  }

  /**
   * 更新哈希
   * @param v 需要计算哈希的任意类型数据
   * @param hash_code 接收哈希值。如果该值不为0，则哈希值是增量的。
   */
  template <class T> static void update(T&& v, uint64_t& hash_code) {
    update(&v, sizeof(T), hash_code);
  }
};
} // namespace Utility
} // namespace v1_2_x
} // namespace SrhinoPluginFramework