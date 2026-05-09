#pragma once

#include "source/common/common/logger.h"
#include "envoy/srhino_plugin_framework/v1_3_x/libs/regex/regex_replacer.h"
#include "regex_utility.hpp"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
namespace Regex {

class RegexReplacerImpl : public RegexReplacer,
                          public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  RegexReplacerImpl(const RegexReplacerImpl&) = delete;
  RegexReplacerImpl(bool ignore_case, const std::string& match_expr,
                    const std::string& replace_expr, EncodingType type);
  ~RegexReplacerImpl() = default;

public:
  bool replace(std::string& in, EncodingType code) override;
  bool ignore_case() const override { return ignore_case_; }
  const std::string& orig_match_expr() const override { return orig_match_expr_; }
  const std::string& replace_expr() const override { return replace_expr_; }

private:
  const bool ignore_case_;
  const std::string orig_match_expr_;
  const std::string replace_expr_;
  std::array<std::shared_ptr<re2::RE2>, EncodingType::MAX> match_re2s_;
  std::array<std::string, EncodingType::MAX> match_exprs_;
};

} // namespace Regex
} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework