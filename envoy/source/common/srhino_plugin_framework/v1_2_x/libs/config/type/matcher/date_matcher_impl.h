#pragma once
#include "source/common/router/config_utility.h"
#include <unordered_map>
#include <time.h>
#include "envoy/srhino_plugin_framework/v1_2_x/libs/config/type/matcher/date_matcher.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
struct time_period {
  int time_period_start;
  int time_period_end;
};
class TimeImpl : public Time {
public:
  TimeImpl(const srhino_plugin_framework::v1_2_x::proto::config::type::matcher::Time& matcher);
  std::vector<time_period> buildTimePeriod(
      const srhino_plugin_framework::v1_2_x::proto::config::type::matcher::Time& matcher);

public:
  bool match() const override;

private:
  const std::unordered_map<int, std::string> week_;
  const std::vector<time_period> time_period_;
};
using Timeptr = std::unique_ptr<const TimeImpl>;

class DateMatcherImpl : public DateMatcher {
public:
  DateMatcherImpl(
      const srhino_plugin_framework::v1_2_x::proto::config::type::matcher::DateMatcher& matcher);

public:
  bool match() const override;

private:
  Timeptr buildTime(
      const srhino_plugin_framework::v1_2_x::proto::config::type::matcher::DateMatcher& matcher)
      const;
  time_t transDate(const std::string& date);

private:
  Timeptr time_;
  const time_t exact_date_start_;
  const time_t exact_date_end_;
  bool invert_;
};
using DateMatcherptr = std::unique_ptr<const DateMatcherImpl>;

} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework
