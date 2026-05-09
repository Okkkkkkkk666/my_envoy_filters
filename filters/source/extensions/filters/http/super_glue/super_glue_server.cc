#include "super_glue_server.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace SuperGlueFilter {

const Http::LowerCaseString ServerFilter::header_flag_(SUPER_GLUE_HEADER);

ServerFilterGlobalConfig::ServerFilterGlobalConfig(const v3::SuperGlueServerGlobal&,
                                                   Event::Dispatcher&, Stats::Scope&) {}

ServerFilterRouteConfig::ServerFilterRouteConfig(const v3::SuperGlueServerRoute&,
                                                 Event::Dispatcher&) {}

Http::FilterHeadersStatus ServerFilter::decodeHeaders(Http::RequestHeaderMap& headers,
                                                      bool end_stream) {
  after_request_header_map_ = Http::RequestHeaderMapImpl::create();
  headers.iterate([&](const Envoy::Http::HeaderEntry& header) -> Envoy::Http::HeaderMap::Iterate {
    std::string key = std::string(header.key().getStringView());
    if (key.find(':', 0) != std::string::npos) {
      return Envoy::Http::HeaderMap::Iterate::Continue;
    }
    // 将每个Header添加标记
    std::string new_header_key = request_header_tag + std::string(key);
    after_request_header_map_->addCopy(Http::LowerCaseString(new_header_key),
                                       header.value().getStringView());
    return Envoy::Http::HeaderMap::Iterate::Continue;
  });
  if (end_stream) {
    sendLocalReply(Http::Code::OK);
  }
  return Http::FilterHeadersStatus::StopIteration;
}

Http::FilterDataStatus ServerFilter::decodeData(Buffer::Instance& data, bool end_stream) {
  if (end_stream) {
    decoder_callbacks_->addDecodedData(data, false);
    sendLocalReply(Http::Code::OK, decoder_callbacks_->decodingBuffer()->toString());
  }
  return Http::FilterDataStatus::StopIterationAndBuffer;
}

void ServerFilter::onStreamComplete() {}

void ServerFilter::sendLocalReply(Http::Code status, const std::string& body) {
  decoder_callbacks_->sendLocalReply(
      status, body,
      [this](Http::HeaderMap& headers) {
        after_request_header_map_->iterate([&headers](const Envoy::Http::HeaderEntry& header) {
          headers.addCopy(Http::LowerCaseString(header.key().getStringView()),
                          header.value().getStringView());
          return Envoy::Http::HeaderMap::Iterate::Continue;
        });
        headers.setReferenceKey(header_flag_, "hi" /*only magic value*/);
      },
      absl::nullopt, "");
}
} // namespace SuperGlueFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy