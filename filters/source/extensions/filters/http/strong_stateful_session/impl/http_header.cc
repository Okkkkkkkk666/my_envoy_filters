#include "http_header.h"

#include "source/common/http/headers.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {
namespace Impl {
HttpHeader::HttpHeader(const v3::StrongStatefulSessionGlobal& proto_config)
    : StatefulSession(proto_config), header_(Http::LowerCaseString(proto_config.header().name())) {}
HttpHeader::HttpHeader(const v3::StrongStatefulSessionPerRoute& proto_config)
    : StatefulSession(proto_config), header_(Http::LowerCaseString(proto_config.header().name())) {}

std::string HttpHeader::upstreamAddressInternal(const Http::RequestHeaderMap& headers,
                                                const StreamInfo::StreamInfo& /*stream_info*/) {
  return getHeadValue(headers);
}

std::string HttpHeader::updateInternal(const Upstream::HostDescription&,
                                       const Http::RequestHeaderMap& request_headers,
                                       Http::ResponseHeaderMap& /*response_headers*/,
                                       const StreamInfo::StreamInfo& /*stream_info*/) {
  return getHeadValue(request_headers);
}

std::string HttpHeader::getHeadValue(const Http::RequestHeaderMap& headers) const {
  std::string header_value;
  const auto entry = headers.getByKey(header_);
  if (entry.has_value()) {
    header_value.assign(entry.value().data(), entry.value().length());
  }

  return header_value;
}
} // namespace Impl
} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy