#pragma once
#include <array>
#include <functional>

#include "envoy/network/address.h"
#include "envoy/http/header_map.h"
#include "source/common/http/header_utility.h"
#include "source/common/singleton/threadsafe_singleton.h"
#include "re2/re2.h"

#include "filters/api/envoy/extensions/filters/http/strong_local_ratelimit/v3/strong_local_ratelimit.pb.h"

#include "quota.h"
#include "key.h"
#include "filters/source/extensions/filters/http/common/lru_cache.hpp"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongLocalRateLimitFilter {
namespace Impl {

namespace v3 = envoy::extensions::filters::http::strong_local_ratelimit::v3;

class Action {
public:
  Action(const v3::Action& action);
  Action(const Action&) = delete;

public:
  struct ProcResult {
    // 是否通过
    bool pass{};
    // 当被限速时，流控项当前的状态
    uint32_t duration{};
    uint32_t remaining{};
  };

  /**
   * 执行限速动作
   * @param downstreamAddress 下游地址
   * @param headers 请求头
   * @param vh_name VH 名字
   * @param filter_instance_id 过滤器实例ID
   * @param rule_hash 规则hash
   * @param dryrun 是否空转
   * @param result 执行结果
   */
  void proc(const Network::Address::InstanceConstSharedPtr downstreamAddress,
            Http::RequestHeaderMap& headers, const std::string_view& vh_name,
            uint32_t filter_instance_id, uint64_t rule_hash, bool dryrun, ProcResult& result,
            const std::string& user_name) const;

  // 全局流控项相关方法
public:
  /**
   * 对流控项缓存进行清理（清除太久没有使用的）
   * 注意：
   * 为了不影响并发性能，一次的清理个数不要太大，同时外部定时调用此方法的频率也不需要太频繁。
   * 即使从不调用此方法也可以，这样缓存只会在请求到达且缓存已满时进行自动清理。
   * @param count 清理个数
   */
  static void cleanQuotas(size_t count);

  /**
   * 获取当前流控项缓存数量
   * @return size_t
   */
  static size_t quotasSize();

  /**
   * 获取流控项最大缓存数量
   * @return size_t
   */
  static size_t quotasMaxSize();

  // PB 属性
public:
  v3::Action::Target target() const { return target_; }
  const Http::HeaderUtility::HeaderData& targetHeaderMathcher() const {
    return target_header_mathcher_;
  }
  bool dryrun() const { return dryrun_; }
  bool procNextRule() const { return proc_next_rule_; }
  struct QuotaConfig {
    uint32_t duration{};
    uint32_t max_count{};
  };
  const std::array<QuotaConfig, 2>& quotaConfig() const { return quota_config_; }

private:
  const v3::Action::Target target_;
  const Http::HeaderUtility::HeaderData target_header_mathcher_;
  const bool dryrun_;
  const bool proc_next_rule_;
  const re2::RE2 regex_; // target_header_mathcher_自带的正则匹配器只有全匹配，没有局部提取功能
  const std::array<QuotaConfig, 2> quota_config_;
  static const Http::LowerCaseString header_ratelimit_drop_;

private:
  // 使用单实例流控项（全局）
  using Quotas = std::array<Quota, 2>;
  using SingletonQuotas = ThreadSafeSingleton<Filters::Common::LruCache<Key, Quotas>>;

private:
  Quotas makeQuotas() const;
  Key makeKey(const Network::Address::InstanceConstSharedPtr downstreamAddress,
              const Http::RequestHeaderMap& headers, const std::string_view& vh_name,
              uint32_t filter_instance_id, uint64_t rule_hash, const std::string& user_name) const;

private:
  static void adjustQuotaConfig(std::array<QuotaConfig, 2>& quota_config);
  static void procQuotas(Quotas& quotas, ProcResult& result);
};
} // namespace Impl
} // namespace StrongLocalRateLimitFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy