#pragma once
#include <array>
#include <string>

#include "re2/re2.h"
#include "envoy/network/address.h"
#include "envoy/http/header_map.h"
#include "source/common/http/header_utility.h"

#include "filters/api/envoy/extensions/filters/http/strong_global_ratelimit/v3/strong_global_ratelimit.pb.h"

#include "filters/source/extensions/filters/http/common/ratelimit/impl/ratelimit_policy.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongGlobalRatelimit {
namespace Impl {

namespace v3 = envoy::extensions::filters::http::strong_global_ratelimit::v3;

class Action {
public:
  Action(const v3::Action& action);
  Action(const Action&) = delete;

public:
  std::string makeKey(const Network::Address::InstanceConstSharedPtr downstreamAddress,
                      const Http::RequestHeaderMap& headers, const std::string& vh_name,
                      uint64_t rule_hash, const std::string& invoke_namespace,
                      const std::string& user_name) const;
  Filters::Common::RatelimitClient::Impl::Quotas getQuotas() const { return quotas_; };
  v3::Action::Target target() const { return target_; }
  bool procNextRule() const { return proc_next_rule_; }
  bool dryrun() const { return dryrun_; }

private:
  static void adjustQuotaConfig(std::array<Filters::Common::RatelimitClient::Impl::Quota, 2>& quotas);

private:
  const v3::Action::Target target_;
  const Http::HeaderUtility::HeaderData target_header_mathcher_;
  const bool proc_next_rule_;
  const re2::RE2 regex_;
  const Filters::Common::RatelimitClient::Impl::Quotas quotas_;
  const bool dryrun_;
};
} // namespace Impl
} // namespace StrongGlobalRatelimit
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
