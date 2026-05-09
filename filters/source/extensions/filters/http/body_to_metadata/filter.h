#pragma once

#include <vector>
#include <iostream>

#include "source/extensions/filters/http/common/pass_through_filter_ex.h"
#include "source/common/router/config_impl.h"

#include "filters/source/extensions/filters/http/common/strong_matcher/matcher.h"
#include "filters/source/extensions/filters/http/common/strong_matcher/rule.h"
#include "filters/api/envoy/extensions/filters/http/body_to_metadata/v3/body_to_metadata.pb.h"

#define FILTER_NAME "envoy.filters.http.body-to-metadata.1.0"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace BodyToMetadataFilter {

namespace v3 = envoy::extensions::filters::http::body_to_metadata::v3;

class Rule {
public:
  Rule(const v3::Rule& rule);

public:
  bool match(const Http::RequestHeaderMap& headers) const;
  bool match(const Http::ResponseHeaderMap& headers) const;
  uint32_t maxBodyBytes() const { return max_body_bytes_; }

private:
  const uint32_t max_body_bytes_;
  const std::vector<Filters::Common::StrongMatcher::Matcher> matchers_;
};

using RulePtr = std::unique_ptr<Rule>;
using RuleConstPtr = const std::unique_ptr<Rule>;

// 全局配置
class BodyToMetadataGlobalConfig {
public:
  BodyToMetadataGlobalConfig(
      const envoy::extensions::filters::http::body_to_metadata::v3::BodyToMetadataGlobal& config);

public:
  RuleConstPtr& requetsRule() const { return request_rule_; }
  RuleConstPtr& responseRule() const { return response_rule_; }

private:
  RulePtr request_rule_;
  RulePtr response_rule_;
};

using BodyToMetadataGlobalConfigSharedPtr = std::shared_ptr<BodyToMetadataGlobalConfig>;

class Filter : public Http::PassThroughFilterEx, public Logger::Loggable<Logger::Id::filter> {
public:
  Filter(BodyToMetadataGlobalConfigSharedPtr config,
         const Server::Configuration::ServerFactoryContext& context)
      : PassThroughFilterEx(context), filter_hcm_config_(config) {}

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool) override;
  Http::FilterDataStatus decodeData(Buffer::Instance&, bool) override;

  // Http::StreamEncoderFilter
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool) override;
  Http::FilterDataStatus encodeData(Buffer::Instance&, bool) override;
  void onStreamComplete() override;

  // 本地回复触发
  Http::LocalErrorStatus onLocalReply(const LocalReplyData& reply_data) override;

private:
  void tryReserveBuffer(const Http::HeaderMap& headers, std::string& buffer,
                        uint32_t max_body_bytes);
  void appendBody(const Buffer::Instance& data, std::string& buffer, uint32_t max_body_bytes);

private:
  static const std::string filter_name_;
  BodyToMetadataGlobalConfigSharedPtr filter_hcm_config_;
  bool record_request_{false};
  bool record_response_{false};
  bool is_local_reply_{false};
  std::string request_headers_;
  std::string request_body_;
  std::string response_body_;
};

} // namespace BodyToMetadataFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
