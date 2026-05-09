#pragma once

#include "stateful_session.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {
namespace Impl {
// 基于源IP的会话保持实现类
class SourceIp final : public StatefulSession {
public:
  SourceIp(const v3::StrongStatefulSessionGlobal& proto_config);
  SourceIp(const v3::StrongStatefulSessionPerRoute& proto_config);

public:
  std::string upstreamAddressInternal(const Http::RequestHeaderMap& headers,
                                      const StreamInfo::StreamInfo& stream_info) override;
  std::string updateInternal(const Upstream::HostDescription&,
                             const Http::RequestHeaderMap& request_headers,
                             Http::ResponseHeaderMap& response_headers,
                             const StreamInfo::StreamInfo& stream_info) override;

private:
  std::string getSourceIp(const StreamInfo::StreamInfo& stream_info);

private:
  uint32_t prefix_len_;
};
} // namespace Impl
} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy