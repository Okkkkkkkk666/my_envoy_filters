#include "cookie.h"

#include "source/common/http/utility.h"
#include "source/common/http/headers.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace StrongStatefulSessionFilter {
namespace Impl {
const std::string Cookie::magic_prefix_uuid("cfa12fd9968c430480938939af8cea48");

Cookie::Cookie(const v3::StrongStatefulSessionGlobal& proto_config)
    : StatefulSession(proto_config), cookie_(proto_config.cookie()),
      max_age_(proto_config.cookie().session() ? std::chrono::seconds(0) : ttl_) {}
Cookie::Cookie(const v3::StrongStatefulSessionPerRoute& proto_config)
    : StatefulSession(proto_config), cookie_(proto_config.cookie()),
      max_age_(proto_config.cookie().session() ? std::chrono::seconds(0) : ttl_) {}

std::string Cookie::upstreamAddressInternal(const Http::RequestHeaderMap& headers,
                                            const StreamInfo::StreamInfo& /*stream_info*/) {
  std::string cookie_value = Http::Utility::parseCookieValue(headers, cookie_.name());
  if (cookie_value.empty()) {
    return cookie_value;
  }

  // 提取附加值，并还原cookie值
  if (cookie_value.find(magic_prefix_uuid) == 0) {
    std::string real_cookie_value;
    size_t pos = cookie_value.find("_");
    if (pos != cookie_value.npos) {
      real_cookie_value = cookie_value.substr(pos + 1);
      cookie_value =
          cookie_value.substr(magic_prefix_uuid.length(), pos - magic_prefix_uuid.length());
      replaceCookieValue(const_cast<Http::RequestHeaderMap&>(headers), cookie_.name(),
                         real_cookie_value);
    }
  }

  return cookie_value;
}

std::string Cookie::updateInternal(const Upstream::HostDescription& host,
                                   const Http::RequestHeaderMap& request_headers,
                                   Http::ResponseHeaderMap& response_headers,
                                   const StreamInfo::StreamInfo& /*stream_info*/) {
  using namespace std::chrono;

  // 为防止host_address明文泄漏造成被绕过代理的风险，需将host_address哈希后写入cookie
  std::string host_address = host.address()->asString();
  std::string cookie_value = std::to_string(std::hash<std::string>()(host_address));
  std::string cookie_header_value = makeSetCookieValue(
      cookie_.name(), cookie_value, cookie_.domain(), cookie_.path(), max_age_, true);

  // 当前的cookie尚未失效，不需更新cookie
  // 需要注意，当真实业务的set-cookie字段名跟cookie_.name()相同时，需要强制更新cookie，防止真实业务设置的
  // cookie造成会话保持功能失效
  if (Http::Utility::parseSetCookieValue(response_headers, cookie_.name()).empty()) {
    const std::string request_cookie_value =
        Http::Utility::parseCookieValue(request_headers, cookie_.name());
    if (request_cookie_value == cookie_value) {
      return cookie_value;
    }
  }

  // 更新cookie
  if (cookie_.has_insert()) {
    insertCookie(response_headers, cookie_header_value);
  } else if (cookie_.has_rewrite()) {
    std::string new_cookie_value = magic_prefix_uuid + cookie_value;
    rewriteCookie(response_headers, cookie_header_value, new_cookie_value);
  } else {
    std::string value = readCookie(response_headers, cookie_header_value);
    if (!value.empty()) {
      cookie_value = value;
    }
  }

  return cookie_value;
}

void Cookie::insertCookie(Http::ResponseHeaderMap& response_headers,
                          const std::string& cookie_header_value) const {
  removeSetCookie(response_headers, cookie_.name());
  response_headers.addReferenceKey(Http::Headers::get().SetCookie, cookie_header_value);
}

void Cookie::rewriteCookie(Http::ResponseHeaderMap& response_headers,
                           const std::string& cookie_header_value,
                           const std::string& cookie_value) const {
  std::string new_value =
      replaceSetCookieValue(response_headers, cookie_.name(), cookie_value, max_age_);
  if (new_value.empty()) {
    response_headers.addReferenceKey(Http::Headers::get().SetCookie, cookie_header_value);
  }
}

std::string Cookie::readCookie(Http::ResponseHeaderMap& response_headers,
                               const std::string& cookie_header_value) const {
  const std::string cookie_value =
      Http::Utility::parseSetCookieValue(response_headers, cookie_.name());
  if (cookie_value.empty()) {
    response_headers.addReferenceKey(Http::Headers::get().SetCookie, cookie_header_value);
  }

  return cookie_value;
}

void Cookie::removeSetCookie(Http::ResponseHeaderMap& response_headers, const std::string& name) {
  Http::HeaderMap::GetResult result = response_headers.get(Http::Headers::get().SetCookie);

  std::vector<std::string> other_cookie_header_values;
  for (size_t i = 0; i < result.size(); ++i) {
    std::string value = parseSetCookieValue(result[i]->value().getStringView(), name);
    if (value.empty()) {
      other_cookie_header_values.emplace_back(result[i]->value().getStringView());
    }
  }

  // 删除所有set-cookie
  while (true) {
    if (response_headers.remove(Http::Headers::get().SetCookie) == 0) {
      break;
    }
  }

  // 还原其他set-cookie
  for (auto& header_value : other_cookie_header_values) {
    response_headers.addReferenceKey(Http::Headers::get().SetCookie, header_value);
  }
}

std::string Cookie::replaceSetCookieValue(Http::ResponseHeaderMap& response_headers,
                                          const std::string& name, const std::string& value,
                                          const std::chrono::seconds max_age) {
  Http::HeaderMap::GetResult result = response_headers.get(Http::Headers::get().SetCookie);
  std::string new_cookie_header_value;
  std::string new_cookie_value;
  for (size_t i = 0; i < result.size(); ++i) {
    auto cookie_header_value = result[i]->value().getStringView();
    std::string v = parseSetCookieValue(cookie_header_value, name);
    if (!v.empty()) {
      // 重新构造set-cookie的值
      bool has_max_age = false;
      forEachCookieAttr(cookie_header_value, [&](absl::string_view k, absl::string_view v) {
        if (!new_cookie_header_value.empty()) {
          new_cookie_header_value.append(";");
        }

        new_cookie_header_value.append(k.data(), k.length());
        new_cookie_header_value.append("=");
        if (k == name) {
          new_cookie_value = value + "_";
          new_cookie_value.append(v.data(), v.length());
          new_cookie_header_value.append(new_cookie_value);
        } else {
          std::string lowerCaseKey(k.data(), k.length());
          std::transform(lowerCaseKey.begin(), lowerCaseKey.end(), lowerCaseKey.begin(),
                         [](unsigned char c) { return std::tolower(c); });

          if (lowerCaseKey == "max-age") {
            has_max_age = true;
            new_cookie_header_value.append(std::to_string(max_age.count()));
          } else {
            new_cookie_header_value.append(v.data(), v.length());
          }
        }
        return true;
      });

      if (!has_max_age) {
        new_cookie_header_value.append("; Max-Age=");
        new_cookie_header_value.append(std::to_string(max_age.count()));
      }

      break;
    }
  }

  // 重新添加set-cookie
  if (!new_cookie_header_value.empty()) {
    removeSetCookie(response_headers, name);
    response_headers.addReferenceKey(Http::Headers::get().SetCookie, new_cookie_header_value);
  }

  return new_cookie_header_value;
}

void Cookie::replaceCookieValue(Http::RequestHeaderMap& request_headers, const std::string& name,
                                const std::string& value) {
  Http::HeaderMap::GetResult result = request_headers.get(Http::Headers::get().Cookie);
  std::string new_cookie_header_value;
  for (size_t i = 0; i < result.size(); ++i) {
    absl::string_view cookie_header_value_view = result[i]->value().getStringView();
    size_t name_pos = cookie_header_value_view.find(name + "=");
    if (name_pos == std::string::npos) {
      continue;
    }

    // 兼容不规则的格式： "other=xx;name=value"  or "other=xx; name=value"
    if (name_pos > 0) {
      /*防止匹配上后缀跟name相同的字段*/
      if ((cookie_header_value_view.at(name_pos - 1) != ';') && (
           cookie_header_value_view.at(name_pos - 1) != ' ')) {
        continue;
      }
    }

    size_t assign_pos = cookie_header_value_view.find("=", name_pos);
    if (assign_pos != std::string::npos) {
      std::string cookie_header_value(cookie_header_value_view.data(),
                                      cookie_header_value_view.length());
      size_t split_pos = cookie_header_value.find(";", name_pos);
      cookie_header_value.replace(assign_pos + 1, split_pos - (assign_pos + 1), value);
      const_cast<Http::HeaderEntry*>(result[i])->value().setCopy(cookie_header_value);
    }
  }
}

std::string Cookie::parseSetCookieValue(const absl::string_view& cookie_header_value,
                                        const std::string& name) {
  std::string value;

  // 遍历所有键值对，找到key为name的值
  forEachCookieAttr(cookie_header_value, [&](absl::string_view k, absl::string_view v) {
    if (k == name) {
      value.assign(v.data(), v.length());
      return false;
    }

    return true;
  });

  return value;
}

std::string Cookie::makeSetCookieValue(const std::string& key, const std::string& value,
                                       const std::string& domain, const std::string& path,
                                       const std::chrono::seconds max_age, bool httponly) {
  std::string cookie_value;
  // Best effort attempt to avoid numerous string copies.
  cookie_value.reserve(value.size() + path.size() + 30);

  cookie_value = absl::StrCat(key, "=\"", value, "\"");
  if (max_age != std::chrono::seconds::zero()) {
    absl::StrAppend(&cookie_value, "; Max-Age=", max_age.count());
  }
  if (!domain.empty()) {
    absl::StrAppend(&cookie_value, "; Domain=", domain);
  }
  if (!path.empty()) {
    absl::StrAppend(&cookie_value, "; Path=", path);
  }
  if (httponly) {
    absl::StrAppend(&cookie_value, "; HttpOnly");
  }
  return cookie_value;
}

void Cookie::forEachCookieAttr(
    const absl::string_view& cookie_header_value,
    const std::function<bool(absl::string_view, absl::string_view)>& cookie_consumer) {
  // Split the cookie header into individual cookies.
  for (const auto& s : StringUtil::splitToken(cookie_header_value, ";")) {
    // Find the key part of the cookie (i.e. the name of the cookie).
    size_t first_non_space = s.find_first_not_of(' ');
    size_t equals_index = s.find('=');
    if (equals_index == absl::string_view::npos) {
      // The cookie is malformed if it does not have an `=`. Continue
      // checking other cookies in this header.
      continue;
    }
    absl::string_view k = s.substr(first_non_space, equals_index - first_non_space);
    absl::string_view v = s.substr(equals_index + 1, s.size() - 1);

    // Cookie values may be wrapped in double quotes.
    // https://tools.ietf.org/html/rfc6265#section-4.1.1
    if (v.size() >= 2 && v.back() == '"' && v[0] == '"') {
      v = v.substr(1, v.size() - 2);
    }

    if (!cookie_consumer(k, v)) {
      return;
    }
  }
}

} // namespace Impl
} // namespace StrongStatefulSessionFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy