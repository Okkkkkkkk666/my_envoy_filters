#pragma once
#include <string>

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {
namespace Impl {
// 流控项Key
class Key {
public:
  Key() = default;
  Key(const Key& key);
  Key(Key&& key) noexcept;

public:
  /**
   * 追加一段数据
   * @param data
   * @param len
   */
  void append(const void* data, size_t len);

  /**
   * 获取哈希值
   * @return size_t
   */
  size_t hash() const;

  bool operator==(const Key& key) const;

private:
  std::string buffer_;
};
} // namespace Impl
} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy

namespace std {
template <> struct hash<Envoy::Extensions::HttpFilters::StrongLocalRateLimitFilter::Impl::Key> {
  using ArgumentType = Envoy::Extensions::HttpFilters::StrongLocalRateLimitFilter::Impl::Key;
  using ResultType = std::size_t;

  ResultType operator()(const ArgumentType& o) const { return o.hash(); }
};

} // namespace std