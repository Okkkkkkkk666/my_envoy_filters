#pragma once

#include <string>
#include <array>

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace RatelimitClient {
namespace Impl {

struct Quota {
  uint32_t duration_{};
  uint32_t max_count_{};
};
using Quotas = std::array<Quota, 2>;

class RateLimitPolicy {
public:
  RateLimitPolicy(const std::string key, std::array<Quota, 2> quotas, uint32_t block_time,
                  bool add_headers = false, bool dryrun = false)
      : key_(key), quotas_(quotas), block_time_(block_time), add_headers_(add_headers),
        dryrun_(dryrun){};
  RateLimitPolicy(const std::string& key, std::array<Quota, 2> quotas, uint32_t block_time,
                  const std::string& rule_name, bool add_headers, bool dryrun)
      : key_(key), quotas_(quotas), block_time_(block_time), add_headers_(add_headers),
        dryrun_(dryrun), rule_name_(rule_name){};
  RateLimitPolicy(const RateLimitPolicy& policy) = delete;
  const std::string& key() const { return key_; }
  const Quotas& quotas() const { return quotas_; }
  bool addHeaders() const { return add_headers_; }
  uint32_t blockTime() const { return block_time_; }
  bool dryrun() const { return dryrun_; }
  const std::string& ruleName() const { return rule_name_; }

private:
  const std::string key_;
  const Quotas quotas_;
  const uint32_t block_time_;
  const bool add_headers_;
  const bool dryrun_;
  std::string rule_name_;
};
using RateLimitPolicyPtr = std::unique_ptr<RateLimitPolicy>;
using RateLimitPolicySharedPtr = std::shared_ptr<RateLimitPolicy>;

} // namespace Impl
} // namespace RatelimitClient
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
