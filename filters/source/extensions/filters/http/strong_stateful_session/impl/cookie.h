#pragma once

#include "stateful_session.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {
namespace Impl {
std::string test_parseSetCookieValue(const absl::string_view& cookie_header_value, const std::string& name);
// 基于cookie的会话保持实现类
class Cookie final : public StatefulSession {
  friend std::string test_parseSetCookieValue(const absl::string_view& cookie_header_value, const std::string& name);
public:
  Cookie(const v3::StrongStatefulSessionGlobal& proto_config);
  Cookie(const v3::StrongStatefulSessionPerRoute& proto_config);

public:
  std::string upstreamAddressInternal(const Http::RequestHeaderMap& headers,
                                      const StreamInfo::StreamInfo& stream_info) override;
  std::string updateInternal(const Upstream::HostDescription& host,
                             const Http::RequestHeaderMap& request_headers,
                             Http::ResponseHeaderMap& response_headers,
                             const StreamInfo::StreamInfo& stream_info) override;

private:
  // 插入，新增或覆盖已存在的
  void insertCookie(Http::ResponseHeaderMap& response_headers,
                    const std::string& cookie_header_value) const;
  // 重写，新增或在已存在的值里面附加
  void rewriteCookie(Http::ResponseHeaderMap& response_headers,
                     const std::string& cookie_header_value, const std::string& cookie_value) const;
  // 读取，新增或读取现有的
  std::string readCookie(Http::ResponseHeaderMap& response_headers,
                         const std::string& cookie_header_value) const;

private:
  static void removeSetCookie(Http::ResponseHeaderMap& response_headers, const std::string& name);
  static std::string replaceSetCookieValue(Http::ResponseHeaderMap& response_headers,
                                           const std::string& name, const std::string& value,
                                           const std::chrono::seconds max_age);
  static void replaceCookieValue(Http::RequestHeaderMap& request_headers, const std::string& name,
                                 const std::string& value);
  static std::string parseSetCookieValue(const absl::string_view& cookie_header_value,
                                         const std::string& name);
  static std::string makeSetCookieValue(const std::string& key, const std::string& value,
                                        const std::string& domain, const std::string& path,
                                        const std::chrono::seconds max_age, bool httponly);
  static void forEachCookieAttr(
      const absl::string_view& cookie_header_value,
      const std::function<bool(absl::string_view, absl::string_view)>& cookie_consumer);

private:
  v3::Cookie cookie_;
  // cookie有效时间，当开启会话cookie时，此值为0
  std::chrono::seconds max_age_;
  // cookie值插入标识，用于识别插入的cookie值，以便还原原来的cookie值
  static const std::string magic_prefix_uuid;
};
} // namespace Impl
} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy