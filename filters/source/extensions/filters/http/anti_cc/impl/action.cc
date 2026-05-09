#include "action.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AntiCC {
namespace Impl {

const uint16_t Action::auto_level_high_block_time_(1800);
const uint16_t Action::auto_level_medium_block_time_(1200);
const uint16_t Action::auto_level_low_block_time_(600);
const RatelimitImpl::Quotas Action::auto_level_high_quotas_ = {RatelimitImpl::Quota{5, 200},
                                                               RatelimitImpl::Quota{60, 1200}};
const RatelimitImpl::Quotas Action::auto_level_medium_quotas_ = {RatelimitImpl::Quota{5, 1000},
                                                                 RatelimitImpl::Quota{60, 2400}};
const RatelimitImpl::Quotas Action::auto_level_low_quotas_ = {RatelimitImpl::Quota{5, 2000},
                                                              RatelimitImpl::Quota{60, 7000}};

Action::Action(const v3::Action& action)
    : dryrun_(action.dryrun()), man_machine_verification_(action.man_machine_verification()),
      all_run_verification_(action.all_run_verification()), rate_limit_quota_([&action]() {
        RatelimitImpl::Quota quota;
        quota.duration_ = action.rate_limit_quota().duration();
        quota.max_count_ = action.rate_limit_quota().max_count();
        RatelimitImpl::Quotas quotas{quota, RatelimitImpl::Quota{0, 0}};
        return quotas;
      }()),
      verify_quota_([&action]() {
        RatelimitImpl::Quota quota;
        quota.duration_ = action.check_quota().duration();
        quota.max_count_ = action.check_quota().max_count();
        return quota;
      }()),
      block_time_(action.block_time()) {}

Action::Action(const v3::Action& action, v3::AutoLevel auto_level)
    : dryrun_(action.dryrun()), man_machine_verification_(action.man_machine_verification()),
      all_run_verification_(action.all_run_verification()), rate_limit_quota_([&auto_level]() {
        switch (auto_level) {
        case v3::HIGH:
          return auto_level_high_quotas_;
        case v3::MEDIUM:
          return auto_level_medium_quotas_;
        case v3::LOW:
          return auto_level_low_quotas_;
        default:
          return RatelimitImpl::Quotas{RatelimitImpl::Quota{0, 0}, RatelimitImpl::Quota{0, 0}};
        }
      }()),
      verify_quota_([&action]() {
        RatelimitImpl::Quota quota;
        quota.duration_ = action.check_quota().duration();
        quota.max_count_ = action.check_quota().max_count();
        return quota;
      }()),
      block_time_([&auto_level]() {
        switch (auto_level) {
        case v3::HIGH:
          return auto_level_high_block_time_;
        case v3::MEDIUM:
          return auto_level_medium_block_time_;
        case v3::LOW:
          return auto_level_low_block_time_;
        default:
          return uint16_t(0);
        }
      }()) {}

} // namespace Impl
} // namespace AntiCC
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy