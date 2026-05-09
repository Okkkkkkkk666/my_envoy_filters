#include "date_matcher_impl.h"
namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
TimeImpl::TimeImpl(
    const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::Time& matcher)
    : week_([&matcher, this]() {
        std::unordered_map<int, std::string> weeks;
        for (std::string week : matcher.week()) {
          if (week == "sun") {
            weeks.insert(std::make_pair(0, week));
          } else if (week == "Mon") {
            weeks.insert(std::make_pair(1, week));
          } else if (week == "Tue") {
            weeks.insert(std::make_pair(2, week));
          } else if (week == "Wed") {
            weeks.insert(std::make_pair(3, week));
          } else if (week == "Thu") {
            weeks.insert(std::make_pair(4, week));
          } else if (week == "Fri") {
            weeks.insert(std::make_pair(5, week));
          } else if (week == "Sta") {
            weeks.insert(std::make_pair(6, week));
          }
        }
        return weeks;
      }()),
      time_period_(buildTimePeriod(matcher)) {}

std::vector<time_period> TimeImpl::buildTimePeriod(
    const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::Time& matcher) {
  std::vector<time_period> time_periods;
  for (auto& time : matcher.time_period()) {
    size_t dash_pos = time.find('-');
    if (dash_pos == std::string::npos)
      continue;

    // 分割起止时间
    std::string start_str = time.substr(0, dash_pos);
    std::string end_str = time.substr(dash_pos + 1);

    // 解析小时和分钟
    int start_h, start_m, end_h, end_m;
    if (sscanf(start_str.c_str(), "%d:%d", &start_h, &start_m) != 2)
      continue;
    if (sscanf(end_str.c_str(), "%d:%d", &end_h, &end_m) != 2)
      continue;

    // 时间有效性检查
    auto validate = [](int h, int m) { return (h >= 0 && h < 24) && (m >= 0 && m < 60); };
    if (!validate(start_h, start_m) || !validate(end_h, end_m))
      continue;

    int start_total = (start_h * 60 + start_m) * 60;
    int end_total = (end_h * 60 + end_m) * 60;
    time_period time_period_;
    time_period_.time_period_end = end_total;
    time_period_.time_period_start = start_total;
    if (time_period_.time_period_start <= time_period_.time_period_end) {
      time_periods.emplace_back(time_period_);
    }
  }
  return time_periods;
}

bool TimeImpl::match() const {
  time_t current_time;
  time(&current_time);
  struct tm local_time;

  // 获取本地时间
  localtime_r(&current_time, &local_time);
  // 匹配星期
  if (!week_.empty()) {
    if (week_.find(local_time.tm_wday) == week_.end()) {
      return false;
    }
  }

  // 匹配时间段
  if (!time_period_.empty()) {
    bool time_matched = false;
    int current_sec = (local_time.tm_hour * 60 + local_time.tm_min) * 60;

    for (const auto& period : time_period_) {
      time_matched =
          (current_sec >= period.time_period_start) && (current_sec < period.time_period_end);
      if (time_matched)
        break;
    }

    if (!time_matched)
      return false;
  }

  return true;
}

DateMatcherImpl::DateMatcherImpl(
    const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::DateMatcher& matcher)
    : time_(buildTime(matcher)), exact_date_start_(transDate(matcher.exact_date_start())),
      exact_date_end_(transDate(matcher.exact_date_end())), invert_(matcher.invert()) {}

Timeptr DateMatcherImpl::buildTime(
    const srhino_plugin_framework::v1_3_x::proto::config::type::matcher::DateMatcher& matcher)
    const {
  if (matcher.has_time()) {
    return std::make_unique<TimeImpl>(matcher.time());
  }

  return nullptr;
}

time_t DateMatcherImpl::transDate(const std::string& date) {
  if (date.empty()) {
    return -1;
  }

  int year, month, day, hour, minute;
  // 解析开始时间
  if (sscanf(date.c_str(), "%d-%d-%d-%d:%d", &year, &month, &day, &hour, &minute) != 5) {
    return false;
  }

  // 时间参数有效性验证
  auto validate = [](int val, int min, int max) { return val >= min && val <= max; };

  bool valid = validate(month, 1, 12) && validate(day, 1, 31) && validate(hour, 0, 23) &&
               validate(minute, 0, 59) && validate(month, 1, 12) && validate(day, 1, 31) &&
               validate(hour, 0, 23) && validate(minute, 0, 59);

  if (!valid)
    return false;

  // 转换为时间戳
  auto to_timestamp = [](int year, int month, int day, int hour, int minute, int sec) {
    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = sec;
    t.tm_isdst = -1;
    return mktime(&t);
  };
  const time_t ts = to_timestamp(year, month, day, hour, minute, 0);
  return ts;
}

bool DateMatcherImpl::match() const {
  bool is_match = false;
  if (time_) {
    if (time_->match()) {
      is_match = true;
    }
  } else if (exact_date_start_ != -1 || exact_date_end_ != -1) {
    // 获取当前时间戳
    const time_t now = time(nullptr);
    is_match = (now >= exact_date_start_) && (now <= exact_date_end_);
  }
  if (invert_) {
    is_match = !is_match;
  }
  return is_match;
}
} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework