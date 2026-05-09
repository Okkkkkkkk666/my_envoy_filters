#pragma once

#include "stateful_session.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {
namespace Impl {
// 基于http头的会话保持实现类
class HttpHeader final : public StatefulSession {
public:
  HttpHeader(const v3::StrongStatefulSessionGlobal& proto_config);
  HttpHeader(const v3::StrongStatefulSessionPerRoute& proto_config);

public:
  std::string upstreamAddressInternal(const Http::RequestHeaderMap& headers,
                                      const StreamInfo::StreamInfo& stream_info) override;
  std::string updateInternal(const Upstream::HostDescription&,
                             const Http::RequestHeaderMap& request_headers,
                             Http::ResponseHeaderMap& response_headers,
                             const StreamInfo::StreamInfo& stream_info) override;
private:
  std::string getHeadValue(const Http::RequestHeaderMap& headers) const;
private:
  const Http::LowerCaseString header_;
};
} // namespace Impl
} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy