#include "bot_detection.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace BotDetection {

const std::string BotDetectionFilter::filter_name_(FILTER_NAME);
const Http::LowerCaseString BotDetectionFilter::header_bot_detection_("X-Bot-Detection");

BotDetectionFilterRouteConfig::BotDetectionFilterRouteConfig(
    const v3::BotDetectionRoute& proto_config, std::string bot_list)
    : none_user_agent_mode_(proto_config.none_user_agent_mode()),
      matcher_(std::make_unique<Impl::Matcher>(proto_config.matcher(), bot_list)),
      enable_(proto_config.enable()) {}

Http::FilterHeadersStatus BotDetectionFilter::decodeHeaders(Http::RequestHeaderMap& headers,
                                                            bool /*end_stream*/) {
  // 判断是否存在路由或虚拟主机级别的配置
  const BotDetectionFilterRouteConfig* filter_route_config = getRouteConfig();
  if (filter_route_config == nullptr) {
    ENVOY_LOG(debug, "No VH or Route configuration");
    return Http::FilterHeadersStatus::Continue;
  }
  // 是否启用插件
  if (!filter_route_config->enable()) {
    return Http::FilterHeadersStatus::Continue;
  }

  std::pair<std::string, std::string> user_agent_header;
  user_agent_header.first = "user-agent";
  bool has_user_agent = getUserAgentHeader(user_agent_header, headers);

  if (!has_user_agent) {
    // 请求不携带User-Agent且none_user_agent_mode设为true时，直接放行。
    if (filter_route_config->noneUserAgentMode()) {
      ENVOY_LOG(debug, " No User-Agent header , directly allow");
      return Http::FilterHeadersStatus::Continue;
    } else {
      // 请求不携带User-Agent且none_user_agent_mode设为false时，直接拒绝。
      ENVOY_LOG(debug, "No User-Agent header , directly refuse");
      decoder_callbacks_->sendLocalReply(
          Http::Code::Forbidden, "",
          [](Http::HeaderMap& headers) {
            headers.setReferenceKey(header_bot_detection_, std::string("Deny Access"));
          },
          absl::nullopt, "");
      return Http::FilterHeadersStatus::StopIteration;
    }
  }
  // log_.set_limit(true);

  // 正则匹配User-Agent头
  Impl::Status status = filter_route_config->matcher()->matcherUserAgentHeader(user_agent_header);
  ENVOY_LOG(info, "matcher status={}", status);
  switch (status) {
  case Impl::Status::MATCH_BOT: {
    ENVOY_LOG(debug, "MATCH_BOT");
    log_.set_limit(true);
    decoder_callbacks_->sendLocalReply(
        Http::Code::Forbidden, "",
        [](Http::HeaderMap& headers) {
          headers.setReferenceKey(header_bot_detection_, std::string("Access Denied"));
        },
        absl::nullopt, "");
    return Http::FilterHeadersStatus::StopIteration;
  }
  case Impl::Status::MATCH_DENY: {
    ENVOY_LOG(debug, "MATCH_DENY");
    log_.set_limit(true);
    decoder_callbacks_->sendLocalReply(
        Http::Code::Forbidden, "",
        [](Http::HeaderMap& headers) {
          headers.setReferenceKey(header_bot_detection_, std::string("Access Denied"));
        },
        absl::nullopt, "");
    return Http::FilterHeadersStatus::StopIteration;
  }
  default: {
    log_.set_limit(false);
    return Http::FilterHeadersStatus::Continue;
  }
  }
}

/**
 * 获取请求头中的User-Agent
 * @param user_agent_header 用来存储User-Agent的键值对
 * @param headers HTTP请求头
 */
inline bool
BotDetectionFilter::getUserAgentHeader(std::pair<std::string, std::string>& user_agent_header,
                                       Http::RequestHeaderMap& headers) const {
  Envoy::Http::LowerCaseString key(user_agent_header.first);
  Http::HeaderUtility::GetAllOfHeaderAsStringResult result =
      Http::HeaderUtility::getAllOfHeaderAsString(headers, key);
  if (!result.result().has_value()) {
    return false;
  }
  user_agent_header.second = std::string(result.result().value());
  return true;
}

/**
 * 获取路由级别的配置，若不存在路由级别的配置，则返回虚拟主机级别的配置，若都不存在则返回nullptr
 */
inline const BotDetectionFilterRouteConfig* BotDetectionFilter::getRouteConfig() const {
  const BotDetectionFilterRouteConfig* filter_route_config = nullptr;
  auto route = decoder_callbacks_->route();
  if (route) {
    filter_route_config = dynamic_cast<const BotDetectionFilterRouteConfig*>(
        // 路由级别的配置不存在时，mostSpecificPerFilterConfig返回的是VH级别的配置
        route->mostSpecificPerFilterConfig(filter_name_));
  }

  return filter_route_config;
}

void BotDetectionFilter::onStreamComplete() {
  log(MessageUtil::getJsonStringFromMessageOrDie(log_, false, true));
}

} // namespace BotDetection
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy