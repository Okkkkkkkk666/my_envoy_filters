#pragma once

#include "filters/api/envoy/extensions/filters/http/waf/v3/waf.pb.h"
#include "filters/api/envoy/extensions/filters/http/waf/v3/waf_log.pb.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ApiCheckFilter {
namespace Impl {

constexpr uint64_t all_rule_type = 134209535;
namespace waf_v3 = envoy::extensions::filters::http::waf::v3;

class WafCheckModeConfig {
public:
  WafCheckModeConfig()
      : paranoia_level(waf_v3::WafGlobal::ParanoiaLevel::WafGlobal_ParanoiaLevel_L4),
        ruly_types(all_rule_type), detect_only(true),
        mode(waf_v3::WafGlobal::Moderation::WafGlobal_Moderation_precise) {}
  WafCheckModeConfig(const WafCheckModeConfig& config)
      : paranoia_level(config.paranoia_level), ruly_types(config.ruly_types),
        detect_only(config.detect_only), mode(config.mode) {}
  const waf_v3::WafGlobal::ParanoiaLevel& paranoiaLevel() const { return paranoia_level; }
  uint64_t ruleTypes() const { return ruly_types; }
  bool detectOnly() const { return detect_only; }
  const waf_v3::WafGlobal::Moderation& moderation() const { return mode; }

private:
  const waf_v3::WafGlobal::ParanoiaLevel paranoia_level;
  const uint64_t ruly_types;
  const bool detect_only;
  const waf_v3::WafGlobal::Moderation mode;
};
} // namespace Impl
} // namespace ApiCheckFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy