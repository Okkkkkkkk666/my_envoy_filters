#include <algorithm>

#include "key.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {
namespace Impl {
Key::Key(const Key& key) { buffer_ = key.buffer_; }
Key::Key(Key&& key) noexcept { buffer_.swap(key.buffer_); }
void Key::append(const void* data, size_t len) {
  buffer_.append(reinterpret_cast<const char*>(data), len);
}
bool Key::operator==(const Key& key) const { return buffer_ == key.buffer_; }

size_t Key::hash() const { return std::hash<std::string>()(buffer_); }
} // namespace Impl
} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy