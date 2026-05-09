#include <chrono>
#include <cmath>

#include "quota.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {
namespace Impl {

Envoy::Stats::Gauge* Quota::stats_ = nullptr;

using namespace std::chrono;

Quota::Quota(uint32_t duration, uint32_t max_count)
    : created_tick_(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count()),
      duration_(duration), max_count_(max_count) {
  if (stats_) {
    stats_->inc();
  }
}

Quota::Quota(const Quota& quota)
    : created_tick_(quota.created_tick_), duration_(quota.duration_), max_count_(quota.max_count_),
      last_tick_(quota.last_tick_), pass_count_(quota.pass_count_),
      last_pass_count_(quota.last_pass_count_) {
  if (stats_) {
    stats_->inc();
  }
}

Quota::~Quota() {
  if (stats_) {
    stats_->dec();
  }
}

/**
 * 滑动窗口算法：
 * 前一个窗口放行数量在当前窗口占一定比例，配额算法为：
 * 某一时间点的配额 = 窗口总配额 - 前一窗口占比 * 前一窗口放行数
 * 如果前面的窗口没有放行任何请求，实际上此时当前窗口等同于固定窗口。
 * 一共有以下五种可能：
 * 1:|---now-----------|-----------------|---------------> (last_pass_count == 0)
 * 2:|---last---now----|-----------------|---------------> (last_pass_count == 0)
 * 3:|-----------------|---last---now----|---------------> (last_pass_count > 0)
 * 4:|---last----------|---now-----------|---------------> (last_pass_count > 0)
 * 5:|---last----------|-----------------|---now---------> (last_pass_count == 0)
 */
bool Quota::consume(uint64_t now) {
  uint32_t max_count = max_count_;
  if (last_tick_ != 0) {
    const uint32_t duration = duration_ * 1000; // 转化为毫秒
    uint32_t last_fixed_window_index = (last_tick_ - created_tick_) / duration;
    uint32_t curr_fixed_window_index = (now - created_tick_) / duration;

    if (last_fixed_window_index != curr_fixed_window_index) {
      // 两者相邻？
      if (curr_fixed_window_index - 1 == last_fixed_window_index) {
        last_pass_count_ = pass_count_;
        pass_count_ = 0;

        uint64_t begin = curr_fixed_window_index * duration + created_tick_;
        uint64_t end = begin + duration - 1;

        // 上一固定窗口在滑动窗口中的占比
        double ratio = static_cast<double>(end - now) / duration;

        // 调整当前时间允许的请求个数
        max_count = max_count_ - std::round(last_pass_count_ * ratio);
      } else {
        last_pass_count_ = 0;
        pass_count_ = 0;
      }
    } else {
      if (last_pass_count_ != 0) {
        uint64_t begin = curr_fixed_window_index * duration + created_tick_;
        uint64_t end = begin + duration - 1;

        // 上一固定窗口在滑动窗口中的占比
        double ratio = static_cast<double>(end - now) / duration;

        // 调整当前时间允许的请求个数
        max_count = max_count_ - std::round(last_pass_count_ * ratio);
      }
    }
  }

  last_tick_ = now;

  // 配额是否足够
  if (pass_count_ + 1 <= max_count) {
    ++pass_count_;
    return true;
  }

  return false;
}
} // namespace Impl
} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy