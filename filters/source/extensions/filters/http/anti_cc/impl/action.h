#pragma once

#include "filters/api/envoy/extensions/filters/http/anti_cc/v3/anti_cc.pb.h"

// #include "quota.h"
#include "filters/source/extensions/filters/http/common/ratelimit/impl/ratelimit_policy.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AntiCC {
namespace Impl {

namespace v3 = envoy::extensions::filters::http::anti_cc::v3;
namespace RatelimitImpl = Filters::Common::RatelimitClient::Impl;

class Action {
public:
  Action(const v3::Action& action);
  Action(const v3::Action& action, v3::AutoLevel auto_level);

public:
  bool dryrun() const { return dryrun_; }
  bool manMachineVerification() const { return man_machine_verification_; }
  bool allRunVerification() const { return all_run_verification_; }
  const RatelimitImpl::Quotas& rateLimitQuota() const { return rate_limit_quota_; }
  const RatelimitImpl::Quota& verifyQuota() const { return verify_quota_; }
  uint32_t blockTime() const { return block_time_; }

private:
  const bool dryrun_;
  const bool man_machine_verification_;
  const bool all_run_verification_;
  const RatelimitImpl::Quotas rate_limit_quota_;
  const RatelimitImpl::Quota verify_quota_;
  const uint32_t block_time_;
  static const uint16_t auto_level_high_block_time_;
  static const uint16_t auto_level_medium_block_time_;
  static const uint16_t auto_level_low_block_time_;
  static const RatelimitImpl::Quotas auto_level_high_quotas_;
  static const RatelimitImpl::Quotas auto_level_medium_quotas_;
  static const RatelimitImpl::Quotas auto_level_low_quotas_;
};
using ActionPtr = std::shared_ptr<Action>;
} // namespace Impl
} // namespace AntiCC
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy