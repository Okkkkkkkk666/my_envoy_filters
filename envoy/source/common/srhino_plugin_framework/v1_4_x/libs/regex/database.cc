#include "database.h"

#include <fstream>
#include <regex>

#include <assert.h>
#include <format>
#include <iostream>

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {

static bool isPcre(const std::string& expression) {
  static std::regex is_pcre_pattern(R"(.*(?:\(\?=|\(\?!|\(\?<=|\(\?<!|\(\?>|\\\d|[?*+}]\+).*)");
  return std::regex_match(expression, is_pcre_pattern);
}

bool DataTagRule::init(const RuleConfig& config, bool non_leftmost) {
  if (config.expr.empty()) {
    ENVOY_LOG(warn, "rule expr is empty, name: {}, id: {}", config.name, config.id);
    return false;
  }
  real_ids_ = config.id;
  if (isPcre(config.expr)) {
    // For PCRE rules, we need to compile the pattern separately.
    pcre_pattern_ = std::make_unique<PcrePattern>(config.expr, config.case_less);
    if (!pcre_pattern_->isValid()) {
      ENVOY_LOG(error, "failed to compile PCRE pattern: {}, id: {}", config.expr, config.id);
      return false;
    }
    return true;
  } else {
    if (!initContainConstraints(config.contains, config.case_less)) {
      ENVOY_LOG(warn, "initContainConstraints failed !");
    }
    if (!initBeforeConstraint(config.before, config.case_less)) {
      ENVOY_LOG(warn, "initBeforeConstraint failed !");
    }
    if (!initAfterConstraint(config.after, config.case_less)) {
      ENVOY_LOG(warn, "initAfterConstraint failed !");
    }

    // if enable non leftmost support, and rule is not leftmost, and has before or contain
    // constraint, need compile leftmost db for this rule
    if (non_leftmost) {
      if (!config.left_most && (hasBefore() || hasContain())) {
        std::stringstream expr_ss;
        expr_ss << "(?:" << config.expr << ")$";
        const auto expr = expr_ss.str();
        if (!master_.compile(expr, config.case_less, true)) {
          ENVOY_LOG(warn, "init config expr  failed !");
          return false;
        }
      }
    }
    return true;
  }
}

bool DataTagRule::initBeforeConstraint(const ConstraintRule& config, bool case_less) {
  if (!config.expr.empty()) {
    std::stringstream expr_ss;
    expr_ss << "(?:" << config.expr << ")$";
    auto expr = expr_ss.str();
    return before_.init(expr, config.max_match_length, config.invert, config.match_null, case_less);
  }
  return true;
}

bool DataTagRule::initAfterConstraint(const ConstraintRule& config, bool case_less) {
  if (!config.expr.empty()) {
    std::stringstream expr_ss;
    expr_ss << "^(?:" << config.expr << ")";
    auto expr = expr_ss.str();
    return after_.init(expr, config.max_match_length, config.invert, config.match_null, case_less);
  }
  return true;
}

bool DataTagRule::initContainConstraints(const std::vector<ConstraintRule>& configs,
                                         bool case_less) {
  contains_.clear();
  contains_.reserve(configs.size());
  for (const auto& config : configs) {
    if (config.expr.empty()) {
      continue;
    }
    Constraint c;
    if (c.init(config, case_less)) {
      contains_.emplace_back(std::move(c));
    } else {
      ENVOY_LOG(warn, "initContainConstraints is null !");
    }
  }
  return true;
}

DataTagDatabase::DataTagDatabase(const std::vector<RuleConfig>& configs, bool multi_line,
                                 bool leftmost, bool non_leftmost) {
  std::vector<std::string> exprs;
  std::vector<uint32_t> fake_ids;
  std::vector<unsigned> flags;
  for (const auto& config : configs) {
    if (config.expr.empty()) {
      ENVOY_LOG(warn, "rule expr is empty, rule name: {}, id: {}", config.name, config.id);
      continue;
    }

    DataTagRule rule{};
    if (!rule.init(config, non_leftmost)) {
      ENVOY_LOG(error, "failed to init rule: {}, id: {}", config.name, config.id);
      continue;
    }

    const uint32_t fake_id = static_cast<uint32_t>(rules_.size());
    unsigned flag = HS_FLAG_UTF8 | HS_FLAG_DOTALL;
    if (config.case_less) {
      flag |= HS_FLAG_CASELESS;
    }
    if (multi_line) {
      flag |= HS_FLAG_MULTILINE;
    }

    if (rule.hasPcre()) {
      flag |= HS_FLAG_PREFILTER;
    } else if (config.left_most && (rule.hasBefore() || rule.hasContain())) {
      flag |= HS_FLAG_SOM_LEFTMOST;
    }

    rules_.emplace_back(std::move(rule));
    flags.push_back(flag);
    exprs.push_back(config.expr);
    fake_ids.push_back(fake_id);
  }

  if (exprs.empty()) {
    ENVOY_LOG(debug, "no valid rules found");
    return;
  }

  if (non_leftmost && !non_leftmost_db_.compile(exprs, fake_ids, flags)) {
    ENVOY_LOG(error, "failed to compile rules with out left most");
    return;
  }

  if (leftmost) {
    for (auto& flag : flags) {
      if (!(flag & HS_FLAG_PREFILTER)) {
        flag |= HS_FLAG_SOM_LEFTMOST;
      }
    }
    if (!leftmost_db_.compile(exprs, fake_ids, flags)) {
      ENVOY_LOG(error, "failed to compile rules with left most");
      return;
    }
  }
}

void DataTagDatabase::allocScratch(HsScratch& scratch) const {
  if (!scratch.alloc(leftmost_db_)) {
    ENVOY_LOG(error, "failed to allocate scratch for database with left most");
  }

  if (!scratch.alloc(non_leftmost_db_)) {
    ENVOY_LOG(error, "failed to allocate scratch for database without left most");
  }

  for (const auto& rule : rules_) {
    if (!rule.allocScratch(scratch)) {
      ENVOY_LOG(error, "failed to allocate scratch for rule {}", rule.id());
    }
  }
}

void DataTagDatabase::allocAfterScratch(HsScratch& scratch) const {
  for (const auto& rule : rules_) {
    if (!rule.allocAfterScratch(scratch)) {
      ENVOY_LOG(error, "failed to allocate after scratch for rule {}", rule.id());
    }
  }
}

} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework