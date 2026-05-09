#pragma once

#include "source/common/router/config_utility.h"

#include "envoy/srhino_plugin_framework/v1_4_x/libs/config/type/matcher/query_parameter_matcher.h"
namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
class QueryParameterMatcherImpl : public QueryParameterMatcher {

public:
  QueryParameterMatcherImpl(const srhino_plugin_framework::v1_4_x::proto::config::type::matcher::QueryParameterMatcher& matcher);

public:
  bool matches(const Envoy::Http::Utility::QueryParams& query_parameters) const override;

private:
  Envoy::Router::ConfigUtility::QueryParameterMatcherPtr matcher_;
  bool invert_;
};
using QueryParameterMatcherPtr = std::unique_ptr<const QueryParameterMatcherImpl>;


} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework