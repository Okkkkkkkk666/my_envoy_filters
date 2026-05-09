#include "ssl_session.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {
namespace Impl {
SslSession::SslSession(const v3::StrongStatefulSessionGlobal& proto_config)
    : StatefulSession(proto_config), ssl_session_(proto_config.ssl_session()) {}
SslSession::SslSession(const v3::StrongStatefulSessionPerRoute& proto_config)
    : StatefulSession(proto_config), ssl_session_(proto_config.ssl_session()) {}

std::string SslSession::upstreamAddressInternal(const Http::RequestHeaderMap& /*headers*/,
                                                const StreamInfo::StreamInfo& stream_info) {
  return getSslSessionId(stream_info);
}

std::string SslSession::updateInternal(const Upstream::HostDescription&,
                                       const Http::RequestHeaderMap& /*request_headers*/,
                                       Http::ResponseHeaderMap& /*response_headers*/,
                                       const StreamInfo::StreamInfo& stream_info) {
  return getSslSessionId(stream_info);
}

std::string SslSession::getSslSessionId(const StreamInfo::StreamInfo& stream_info) const {
  std::string ssl_session_id;
  Ssl::ConnectionInfoConstSharedPtr ssl = stream_info.downstreamAddressProvider().sslConnection();
  if (ssl) {
    ssl_session_id = ssl->sessionId();
  }

  return ssl_session_id;
}
} // namespace Impl
} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy