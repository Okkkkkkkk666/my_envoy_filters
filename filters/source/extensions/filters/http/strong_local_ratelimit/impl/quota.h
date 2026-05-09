#pragma once

#include "envoy/stats/stats.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {
namespace Impl {

// 配额
class Quota final {
public:
  Quota(uint32_t duration, uint32_t max_count);
  Quota(const Quota& quota);
  ~Quota();

  /**
   * 消费一个配额
   * @param now 当前系统开机毫秒数
   * @return true 配额足够
   * @return false 配额不足
   */
  bool consume(uint64_t now);

  /**
   * 该配额是否是有效的
   * @return true
   * @return false
   */
  bool valid() const { return duration_ != 0 && max_count_ != 0; }

  // 相关属性
public:
  uint64_t createdTick() const { return created_tick_; }
  uint32_t duration() const { return duration_; }
  uint32_t maxCount() const { return max_count_; }
  uint64_t lastTick() const { return last_tick_; }
  uint32_t passCount() const { return pass_count_; }
  uint32_t lastPassCount() const { return last_pass_count_; }

  // 统计信息
public:
  static Envoy::Stats::Gauge* getStats() { return stats_; }
  static void setStats(Envoy::Stats::Gauge* stats) { stats_ = stats; }

private:
  // 创建时间戳
  const uint64_t created_tick_;
  // 时间段，单位秒
  const uint32_t duration_;
  // 一个时间段允许的最大请求数量
  const uint32_t max_count_;
  // 上次请求处理时间戳
  uint64_t last_tick_{};
  // 当前时间段已放行的请求数量
  uint32_t pass_count_{};
  // 上个时间段已放行的请求数量
  uint32_t last_pass_count_{};

private:
  // admin统计信息
  static Envoy::Stats::Gauge* stats_;
};

} // namespace Impl
} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy