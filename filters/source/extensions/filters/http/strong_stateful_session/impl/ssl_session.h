#pragma once

#include "stateful_session.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {
namespace Impl {
// 基于SSL会话ID的会话保持实现类
class SslSession final : public StatefulSession {
public:
  SslSession(const v3::StrongStatefulSessionGlobal& proto_config);
  SslSession(const v3::StrongStatefulSessionPerRoute& proto_config);

public:
  std::string upstreamAddressInternal(const Http::RequestHeaderMap& headers,
                                      const StreamInfo::StreamInfo& stream_info) override;
  std::string updateInternal(const Upstream::HostDescription&,
                             const Http::RequestHeaderMap& request_headers,
                             Http::ResponseHeaderMap& response_headers,
                             const StreamInfo::StreamInfo& stream_info) override;
private:
  std::string getSslSessionId(const StreamInfo::StreamInfo& stream_info) const;
private:
  v3::SslSession ssl_session_;
};
} // namespace Impl
} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy