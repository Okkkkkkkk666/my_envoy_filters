#pragma once

#include <utility>

#include "re2/re2.h"

#include "filters/api/envoy/extensions/filters/http/bot_detection/v3/bot_detection.pb.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace BotDetection {
namespace Impl {

namespace v3 = envoy::extensions::filters::http::bot_detection::v3;

enum Status {
  // 所有名单都未匹配上的返回值
  MATCH_EMPTY = 0,
  // 匹配上白名单的返回值
  MATCH_ALLOW = 1,
  // 匹配上黑名单的返回值
  MATCH_DENY = 2,
  // 匹配上机器人名单的返回值
  MATCH_BOT = 3,
};

class Matcher {
public:
  Matcher() noexcept = default;
  Matcher(const v3::Matcher& matcher, std::string bot_list);
  Status matcherUserAgentHeader(const std::pair<std::string, std::string>& header);
  const std::vector<std::shared_ptr<re2::RE2>>& allowList() { return allow_list_; }
  const std::vector<std::shared_ptr<re2::RE2>>& denyList() { return deny_list_; }
  const std::vector<std::shared_ptr<re2::RE2>>& botList() { return bot_list_; }

private:
  // 白名单
  std::vector<std::shared_ptr<re2::RE2>> allow_list_;
  // 黑名单
  std::vector<std::shared_ptr<re2::RE2>> deny_list_;
  // 机器人名单
  std::vector<std::shared_ptr<re2::RE2>> bot_list_;
};

using MatcherPtr = std::unique_ptr<Matcher>;

} // namespace Impl
} // namespace BotDetection
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
