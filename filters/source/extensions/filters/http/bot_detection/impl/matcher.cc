#include "absl/strings/str_split.h"

#include "matcher.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace BotDetection {
namespace Impl {

Matcher::Matcher(const v3::Matcher& matcher, std::string bot_list)
    : allow_list_([&matcher]() {
        std::vector<std::shared_ptr<re2::RE2>> regex_list;
        for (const auto& regex : matcher.allow_list()) {
          regex_list.emplace_back(std::make_shared<re2::RE2>(regex));
        }
        return regex_list;
      }()),
      deny_list_([&matcher]() {
        std::vector<std::shared_ptr<re2::RE2>> regex_list;
        for (const auto& regex : matcher.deny_list()) {
          regex_list.emplace_back(std::make_shared<re2::RE2>(regex));
        }
        return regex_list;
      }()),
      bot_list_([&bot_list]() {
        std::vector<std::shared_ptr<re2::RE2>> regex_list;
        std::vector<std::string> res = absl::StrSplit(bot_list, "\n");
        for (const auto& str : res) {
          regex_list.emplace_back(std::make_shared<re2::RE2>(str));
        }
        return regex_list;
      }()) {}

/**
 * 正则匹配黑、白名单和机器人名单
 * @param header HTTP请求中的User-Agent头
 */
Status Matcher::matcherUserAgentHeader(const std::pair<std::string, std::string>& header) {

  for (const auto& regex : allow_list_) {
    // 检查白名单，匹配成功则返回MATCH_ALLOW
    if (re2::RE2::FullMatch(header.second, *regex)) {
      return MATCH_ALLOW;
    }
  }

  for (const auto& regex : deny_list_) {
    // 检查黑名单，匹配成功则返回MATCH_DENY
    if (re2::RE2::PartialMatch(header.second, *regex)) {
      return MATCH_DENY;
    }
  }

  for (const auto& regex : bot_list_) {
    // 检查机器人名单，匹配成功则返回MATCH_BOT
    if (re2::RE2::PartialMatch(header.second, *regex)) {
      return MATCH_BOT;
    }
  }

  return MATCH_EMPTY;
}

} // namespace Impl
} // namespace BotDetection
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
