#pragma once

#include "source/common/common/logger.h"
#include "source/extensions/filters/http/common/pass_through_filter_ex.h"
#include "source/common/router/config_impl.h"
#include "source/common/http/header_utility.h"

#include "filters/api/envoy/extensions/filters/http/bot_detection/v3/bot_detection.pb.h"
#include "filters/api/envoy/extensions/filters/http/bot_detection/v3/bot_detection_log.pb.h"

#include "impl/matcher.h"

#define FILTER_NAME "envoy.filters.http.bot-detection.1.0"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace BotDetection {

namespace v3 = envoy::extensions::filters::http::bot_detection::v3;
// 全局配置
class BotDetectionFilterGlobalConfig : public Router::RouteSpecificFilterConfig {
public:
  BotDetectionFilterGlobalConfig(const v3::BotDetectionGlobal&) {}
};
using FilterGlobalConfigSharedPtr = std::shared_ptr<BotDetectionFilterGlobalConfig>;

// VH及路由配置
class BotDetectionFilterRouteConfig : public Router::RouteSpecificFilterConfig {
public:
  BotDetectionFilterRouteConfig(const v3::BotDetectionRoute& proto_config, std::string bot_list);

public:
  const Impl::MatcherPtr& matcher() const { return matcher_; }
  bool noneUserAgentMode() const { return none_user_agent_mode_; }
  bool enable() const { return enable_; }

private:
  bool none_user_agent_mode_;
  Impl::MatcherPtr matcher_;
  bool enable_;
};

class BotDetectionFilter : public Http::PassThroughFilterEx,
                           public Logger::Loggable<Logger::Id::filter> {
public:
  BotDetectionFilter(FilterGlobalConfigSharedPtr config,
                     const Server::Configuration::ServerFactoryContext& context)
      : Http::PassThroughFilterEx(context), filter_hcm_config_(config) {}
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  void onStreamComplete() override;

private:
  inline bool getUserAgentHeader(std::pair<std::string, std::string>& user_agent_header,
                                 Http::RequestHeaderMap& headers) const;
  inline const BotDetectionFilterRouteConfig* getRouteConfig() const;

private:
  FilterGlobalConfigSharedPtr filter_hcm_config_;
  static const std::string filter_name_;
  static const Http::LowerCaseString header_bot_detection_;
  v3::BotDetectionLog log_{};
};

} // namespace BotDetection
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy