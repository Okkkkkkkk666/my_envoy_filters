#pragma once
#include "hs_base.h"
#include "envoy/srhino_plugin_framework/v1_4_x/libs/regex/regex_matcher.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {

class Constraint {
public:
  Constraint() = default;
  Constraint(Constraint&&) = default;
  Constraint& operator=(Constraint&&) = default;
  ~Constraint() = default;
  bool init(const ConstraintRule& config, bool case_less);
  bool init(const std::string& expr, uint32_t max_match_length, bool invert,
                                  bool match_null, bool case_less);
  bool hasExpr() const { return db_.get() != nullptr; }
  uint32_t maxMatchLength() const { return max_match_length_; }
  bool allocScratch(HsScratch& scratch) const { return scratch.alloc(db_); }

  bool match(const std::string_view& data, const HsScratch& scratch) const;

private:
  static int matchCb(unsigned int id, unsigned long long from, unsigned long long to,
                     unsigned int flags, void* user_data);

private:
  HsDatabase db_;
  uint32_t max_match_length_{0};
  bool invert_{false};
  bool match_null_{false};
};

} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework