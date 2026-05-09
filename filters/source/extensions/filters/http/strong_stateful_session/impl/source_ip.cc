#include "source/common/common/empty_string.h"
#include "source_ip.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {
namespace Impl {
SourceIp::SourceIp(const v3::StrongStatefulSessionGlobal& proto_config)
    : StatefulSession(proto_config), prefix_len_(proto_config.src_ip().prefix_len().value()) {}
SourceIp::SourceIp(const v3::StrongStatefulSessionPerRoute& proto_config)
    : StatefulSession(proto_config), prefix_len_(proto_config.src_ip().prefix_len().value()) {}
    

std::string SourceIp::upstreamAddressInternal(const Http::RequestHeaderMap& /*headers*/,
                                              const StreamInfo::StreamInfo& stream_info) {
  return getSourceIp(stream_info);
}

std::string SourceIp::updateInternal(const Upstream::HostDescription&,
                                     const Http::RequestHeaderMap& /*request_headers*/,
                                     Http::ResponseHeaderMap& /*response_headers*/,
                                     const StreamInfo::StreamInfo& stream_info) {
  return getSourceIp(stream_info);
}

std::string SourceIp::getSourceIp(const StreamInfo::StreamInfo& stream_info) {
  if (stream_info.downstreamAddressProvider().remoteAddress()->ip()->version() ==
      Network::Address::IpVersion::v4) {
    uint32_t downstream_ip =
        stream_info.downstreamAddressProvider().remoteAddress()->ip()->ipv4()->address();
    downstream_ip = ntohl(downstream_ip);
    downstream_ip >>= (32 - prefix_len_);
    downstream_ip <<= (32 - prefix_len_);
    return sockaddrToString(htonl(downstream_ip));
  }
  return Envoy::EMPTY_STRING;
}

} // namespace Impl
} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy