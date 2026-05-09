#include "constraint.h"

#include <assert.h>

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {

bool Constraint::init(const ConstraintRule& config, bool case_less) {
  return init(config.expr, config.max_match_length, config.invert, config.match_null, case_less);
}

bool Constraint::init(const std::string& expr, uint32_t max_match_length, bool invert,
                      bool match_null, bool case_less) {
  if (!db_.compile(expr, case_less, false)) {
    return false;
  }
  max_match_length_ = max_match_length;
  invert_ = invert;
  match_null_ = match_null;
  return true;
}

bool Constraint::match(const std::string_view& data, const HsScratch& scratch) const {
  assert(db_.get());
  if (data.empty()) {
    return match_null_;
  }

  bool match = false;
  const auto rc = hs_scan(db_.get(), data.data(), data.size(), 0, scratch.get(), matchCb, &match);
  if (rc != HS_SUCCESS) {
    // std::cerr << std::format("hs_scan failed with error code: {}", rc) << std::endl;
    return false;
  }
  return (invert_ ^ match);
}

int Constraint::matchCb(unsigned int, unsigned long long, unsigned long long, unsigned int,
                        void* user_data) {
  bool* match = static_cast<bool*>(user_data);
  if (match) {
    *match = true;
  }
  return 0;
}

} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework