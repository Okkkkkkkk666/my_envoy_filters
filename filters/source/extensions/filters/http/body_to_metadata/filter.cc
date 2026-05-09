#include "filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace BodyToMetadataFilter {

const std::string Filter::filter_name_(FILTER_NAME);

BodyToMetadataGlobalConfig::BodyToMetadataGlobalConfig(
    const envoy::extensions::filters::http::body_to_metadata::v3::BodyToMetadataGlobal& config)
    : request_rule_(config.has_request_rule() ? std::make_unique<Rule>(config.request_rule())
                                              : nullptr),
      response_rule_(config.has_response_rule() ? std::make_unique<Rule>(config.response_rule())
                                                : nullptr) {}

Rule::Rule(const v3::Rule& rule)
    : max_body_bytes_(rule.has_max_body_bytes() ? rule.max_body_bytes().value() : 0),
      matchers_([&]() {
        std::vector<Filters::Common::StrongMatcher::Matcher> result;
        const auto& matchers = rule.matchers();
        for (const envoy::extensions::filters::http::common::strong_matcher::v3::Matcher& matcher :
             matchers) {
          result.emplace_back(matcher);
        }
        return result;
      }()) {}

bool Rule::match(const Http::RequestHeaderMap& headers) const {
  bool result = true;
  for (const auto& matcher : matchers_) {
    if (!matcher.matchAll(headers)) {
      result = false;
      break;
    }
  }

  return result;
}

bool Rule::match(const Http::ResponseHeaderMap& headers) const {
  bool result = true;
  for (const auto& matcher : matchers_) {
    if (!matcher.matchHeader(headers)) {
      result = false;
      break;
    }
  }

  return result;
}

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers, bool end_stream) {
  ENVOY_LOG(trace, "body-to-metata decodeHeaders");

  // 填充 headers
  request_headers_.clear();
  request_headers_.reserve(headers.byteSize() + 4 * headers.size());
  headers.iterate([this](const Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
    absl::StrAppend(&request_headers_, header.key().getStringView(), ": ",
                    header.value().getStringView(), "\r\n");
    return Http::HeaderMap::Iterate::Continue;
  });

  auto& rule = filter_hcm_config_->requetsRule();
  if (rule) {
    record_request_ = rule->match(headers);
  }

  // 预分配内存
  if (record_request_) {
    uint32_t max_body_bytes = filter_hcm_config_->requetsRule()->maxBodyBytes();
    if (max_body_bytes > 0) {
      tryReserveBuffer(headers, request_body_, max_body_bytes);
    }

    // 避免当配置本地回复时，envoy不处理请求body
    if (!end_stream) {
      return Http::FilterHeadersStatus::StopIteration;
    }
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus Filter::decodeData(Buffer::Instance& data, bool) {
  ENVOY_LOG(trace, "body-to-metata decodeData");
  if (record_request_) {
    appendBody(data, request_body_, filter_hcm_config_->requetsRule()->maxBodyBytes());
  }

  return Http::FilterDataStatus::Continue;
}

Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap& headers, bool) {
  ENVOY_LOG(trace, "body-to-metata encodeHeaders");
  if(is_local_reply_) {
    ENVOY_LOG(trace, "body-to-metata is_local_reply_");
    return Http::FilterHeadersStatus::Continue;
  }

  auto& rule = filter_hcm_config_->responseRule();
  if (rule) {
    record_response_ = rule->match(headers);
  }

  // 预分配内存
  if (record_response_) {
    uint32_t max_body_bytes = filter_hcm_config_->responseRule()->maxBodyBytes();
    if (max_body_bytes > 0) {
      tryReserveBuffer(headers, response_body_, max_body_bytes);
    }
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus Filter::encodeData(Buffer::Instance& data, bool) {
  ENVOY_LOG(trace, "body-to-metata encodeData");
  if(is_local_reply_) {
    ENVOY_LOG(trace, "body-to-metata is_local_reply_");
    return Http::FilterDataStatus::Continue;
  }

  if (record_response_) {
    appendBody(data, response_body_, filter_hcm_config_->responseRule()->maxBodyBytes());
  }

  return Http::FilterDataStatus::Continue;
}

void Filter::onStreamComplete() {
  if(!request_headers_.empty()) {
    ENVOY_LOG(trace, "body-to-metata request_headers_");
    ProtobufWkt::Struct metadata;
    auto& fields = *metadata.mutable_fields();
    // 代表envoy处理后，插件处理前
    fields["req_headers_pre_plugin_bytes"].set_string_value(std::to_string(request_headers_.size()));
    // 为避免大量数据以值拷贝方式拷贝进metadata，这里存放的数字实际是std::string的指针
    fields["req_headers_pre_plugin"].set_number_value(reinterpret_cast<int64_t>(&request_headers_));

    decoder_callbacks_->streamInfo().setDynamicMetadata(filter_name_, metadata);
  }
  if (!request_body_.empty()) {
    ENVOY_LOG(trace, "body-to-metata request_body_");
    ProtobufWkt::Struct metadata;
    auto& fields = *metadata.mutable_fields();
    fields["request_body_bytes"].set_string_value(std::to_string(request_body_.size()));

    // 为避免大量数据以值拷贝方式拷贝进metadata，这里存放的数字实际是std::string的指针
    fields["request_body"].set_number_value(reinterpret_cast<int64_t>(&request_body_));

    decoder_callbacks_->streamInfo().setDynamicMetadata(filter_name_, metadata);
  }
  if (!response_body_.empty()) {
    ENVOY_LOG(trace, "body-to-metata response_body_");
    ProtobufWkt::Struct metadata;
    auto& fields = *metadata.mutable_fields();
    fields["response_body_bytes"].set_string_value(std::to_string(response_body_.size()));
    
    // 为避免大量数据以值拷贝方式拷贝进metadata，这里存放的数字实际是std::string的指针
    fields["response_body"].set_number_value(reinterpret_cast<int64_t>(&response_body_));

    encoder_callbacks_->streamInfo().setDynamicMetadata(filter_name_, metadata);
  }
}

// 本地回复触发
Http::LocalErrorStatus Filter::onLocalReply(const LocalReplyData& reply_data) {
  ENVOY_LOG(trace, "body-to-metata onLocalReply");
  // 只有在响应阶段本地回复 不会触发 encode* 方法
  if (!reply_data.body_.empty() && response_body_.empty()) {
    ENVOY_LOG(trace, "body-to-metata is_local_reply_ = true;");
    is_local_reply_ = true;
    response_body_.assign(std::string_view(reply_data.body_));
  }

  return Http::LocalErrorStatus::Continue;
}

void Filter::tryReserveBuffer(const Http::HeaderMap& headers, std::string& buffer,
                              uint32_t max_body_bytes) {
  // 获取Content-Length
  Http::HeaderMap::GetResult result = headers.get(Http::Headers::get().ContentLength);
  uint32_t content_length = 0;
  if (!result.empty()) {
    if (!absl::SimpleAtoi(result[0]->value().getStringView(), &content_length)) {
      return;
    }
  }

  if (content_length > 0 && max_body_bytes > 0) {
    uint32_t reserve = content_length > max_body_bytes ? max_body_bytes : content_length;
    buffer.reserve(reserve);
  }
}

void Filter::appendBody(const Buffer::Instance& data, std::string& buffer,
                        uint32_t max_body_bytes) {
  for (const Buffer::RawSlice& slice : data.getRawSlices()) {
    if (max_body_bytes > 0 && buffer.size() >= max_body_bytes) {
      break;
    }

    uint32_t copy_len = slice.len_;
    if (max_body_bytes > 0 && buffer.size() + copy_len > max_body_bytes) {
      copy_len = max_body_bytes - buffer.size();
    }

    buffer.append(reinterpret_cast<const char*>(slice.mem_), copy_len);
  }
}

} // namespace BodyToMetadataFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
