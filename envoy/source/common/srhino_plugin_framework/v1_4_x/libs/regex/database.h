#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <assert.h>

#include "constraint.h"
#include "hs_base.h"
#include "pcre.h"
#include "envoy/srhino_plugin_framework/v1_4_x/libs/regex/regex_matcher.h"
#include "source/common/common/logger.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {

class DataTagRule : public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  DataTagRule() = default;
  DataTagRule(DataTagRule&&) = default;
  DataTagRule& operator=(DataTagRule&&) = default;
  ~DataTagRule() = default;

  bool init(const RuleConfig& config, bool non_leftmost);
  uint64_t id() const { return real_ids_; }
  const std::vector<Constraint>& contains() const { return contains_; }
  const Constraint& before() const { return before_; }
  const Constraint& after() const { return after_; }

  bool allocScratch(HsScratch& scratch) const {
    for (const auto& c : contains_) {
      if (!c.allocScratch(scratch)) {
        return false;
      }
    }
    return scratch.alloc(master_) && before_.allocScratch(scratch);
  }
  bool allocAfterScratch(HsScratch& scratch) const { return after_.allocScratch(scratch); }

  bool hasContain() const { return !contains_.empty(); }
  bool hasBefore() const { return before_.hasExpr(); }
  bool hasAfter() const { return after_.hasExpr(); }

  bool hasPcre() const { return pcre_pattern_ != nullptr; }
  const PcrePattern* pcre_pattern() const { return pcre_pattern_.get(); }

  const HsDatabase& master() const { return master_; }

private:
  bool initBeforeConstraint(const ConstraintRule& config, bool case_less);
  bool initAfterConstraint(const ConstraintRule& config, bool case_less);
  bool initContainConstraints(const std::vector<ConstraintRule>& configs, bool case_less);

private:
  uint64_t real_ids_{};
  std::vector<Constraint> contains_;
  Constraint before_;
  Constraint after_;

  HsDatabase master_;

  std::unique_ptr<const PcrePattern> pcre_pattern_;
};

class DataTagDatabase : public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  DataTagDatabase(const std::vector<RuleConfig>& onfigs, bool multi_line = false,
                  bool leftmost = true, bool non_leftmost = true);
  ~DataTagDatabase() = default;

  const std::vector<DataTagRule>& rules() const { return rules_; }
  const DataTagRule& rule(uint32_t id) const {
    assert(id < rules_.size());
    if (id < rules_.size()) {
      return rules_[id];
    }
    static const DataTagRule empty_rule;
    return empty_rule;
  }
  void allocScratch(HsScratch& scratch) const;
  void allocAfterScratch(HsScratch& scratch) const;

  const HsDatabase& leftmostDb() const { return leftmost_db_; }
  const HsDatabase& nonLeftmostDb() const { return non_leftmost_db_; }

private:
  std::vector<DataTagRule> rules_; // index: hs id

  HsDatabase leftmost_db_;
  HsDatabase non_leftmost_db_;
};
using DataTagDatabaseConstSharedPtr = std::shared_ptr<const DataTagDatabase>;

} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework