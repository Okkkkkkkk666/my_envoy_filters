#pragma once

#include <algorithm>
#include <chrono>
#include <experimental/functional>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "string_util.hpp"
#include "url.hpp"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Utility {

enum EncodingType { ISO_8859_1, UTF8, GBK, GB2312, MAX, UNKNOWN = MAX };
const char kToUpper[256] = {
    '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07', '\x08', '\x09', '\x0a', '\x0b',
    '\x0c', '\x0d', '\x0e', '\x0f', '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17',
    '\x18', '\x19', '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f', '\x20', '\x21', '\x22', '\x23',
    '\x24', '\x25', '\x26', '\x27', '\x28', '\x29', '\x2a', '\x2b', '\x2c', '\x2d', '\x2e', '\x2f',
    '\x30', '\x31', '\x32', '\x33', '\x34', '\x35', '\x36', '\x37', '\x38', '\x39', '\x3a', '\x3b',
    '\x3c', '\x3d', '\x3e', '\x3f', '\x40', '\x41', '\x42', '\x43', '\x44', '\x45', '\x46', '\x47',
    '\x48', '\x49', '\x4a', '\x4b', '\x4c', '\x4d', '\x4e', '\x4f', '\x50', '\x51', '\x52', '\x53',
    '\x54', '\x55', '\x56', '\x57', '\x58', '\x59', '\x5a', '\x5b', '\x5c', '\x5d', '\x5e', '\x5f',
    '\x60', 'A',    'B',    'C',    'D',    'E',    'F',    'G',    'H',    'I',    'J',    'K',
    'L',    'M',    'N',    'O',    'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W',
    'X',    'Y',    'Z',    '\x7b', '\x7c', '\x7d', '\x7e', '\x7f', '\x80', '\x81', '\x82', '\x83',
    '\x84', '\x85', '\x86', '\x87', '\x88', '\x89', '\x8a', '\x8b', '\x8c', '\x8d', '\x8e', '\x8f',
    '\x90', '\x91', '\x92', '\x93', '\x94', '\x95', '\x96', '\x97', '\x98', '\x99', '\x9a', '\x9b',
    '\x9c', '\x9d', '\x9e', '\x9f', '\xa0', '\xa1', '\xa2', '\xa3', '\xa4', '\xa5', '\xa6', '\xa7',
    '\xa8', '\xa9', '\xaa', '\xab', '\xac', '\xad', '\xae', '\xaf', '\xb0', '\xb1', '\xb2', '\xb3',
    '\xb4', '\xb5', '\xb6', '\xb7', '\xb8', '\xb9', '\xba', '\xbb', '\xbc', '\xbd', '\xbe', '\xbf',
    '\xc0', '\xc1', '\xc2', '\xc3', '\xc4', '\xc5', '\xc6', '\xc7', '\xc8', '\xc9', '\xca', '\xcb',
    '\xcc', '\xcd', '\xce', '\xcf', '\xd0', '\xd1', '\xd2', '\xd3', '\xd4', '\xd5', '\xd6', '\xd7',
    '\xd8', '\xd9', '\xda', '\xdb', '\xdc', '\xdd', '\xde', '\xdf', '\xe0', '\xe1', '\xe2', '\xe3',
    '\xe4', '\xe5', '\xe6', '\xe7', '\xe8', '\xe9', '\xea', '\xeb', '\xec', '\xed', '\xee', '\xef',
    '\xf0', '\xf1', '\xf2', '\xf3', '\xf4', '\xf5', '\xf6', '\xf7', '\xf8', '\xf9', '\xfa', '\xfb',
    '\xfc', '\xfd', '\xfe', '\xff',
};

/**
 * http解析相关处理函数
 */
class HttpParser {
public:
  static EncodingType getEncodeType(const std::string_view& content_type) {
    std::string str(content_type.data(), content_type.length());
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    if (str.find("utf-8") != std::string::npos) {
      return EncodingType::UTF8;
    } else if (str.find("gbk") != std::string::npos) {
      return EncodingType::GBK;
    } else if (str.find("gb2312") != std::string::npos) {
      return EncodingType::GB2312;
    }
    return EncodingType::ISO_8859_1;
  }

  static void forEachCookieAttr(
      const std::string_view& cookie_header_value,
      const std::function<bool(std::string_view, std::string_view)>& cookie_consumer) {
    for (const auto& s : StringUtil::splitToken(cookie_header_value, ";")) {
      size_t first_non_space = s.find_first_not_of(' ');
      size_t equals_index = s.find('=');
      if (equals_index == std::string_view::npos) {
        continue;
      }
      std::string_view k = s.substr(first_non_space, equals_index - first_non_space);
      std::string_view v = s.substr(equals_index + 1, s.size() - 1);

      if (v.size() >= 2 && v.back() == '"' && v[0] == '"') {
        v = v.substr(1, v.size() - 2);
      }

      if (!cookie_consumer(k, v)) {
        return;
      }
    }
  }

  /**
   * 从cookie中提取指定key对应的value
   * @param cookie_header_value cookie请求头的内容
   * @param cookie_key  cookie key
   * @param cookie_value  cookie value
   * @return true 未找到，需要继续查找
   *         false 找到，可以结束查找。cookie_value被设置为找到的 cookie value
   */
  static bool parseCookieHeaderValue(const std::string_view& cookie_header_value,
                                     const std::string_view& cookie_key,
                                     std::string& cookie_value) {
    bool continue_parse = true;
    forEachCookieAttr(cookie_header_value, [&](std::string_view k, std::string_view v) {
      if (k == cookie_key) {
        cookie_value = std::string{v};
        continue_parse = false;
        return false;
      }
      return true;
    });
    return continue_parse;
  }

  static std::string makeSetCookieValue(const std::string& key, const std::string& value,
                                        const std::string& domain, const std::string& path,
                                        const std::chrono::seconds max_age, bool httponly) {
    std::string cookie_value;
    // Best effort attempt to avoid numerous string copies.
    cookie_value.reserve(value.size() + path.size() + 30);

    cookie_value.append(key).append("=\"").append(value).append("\"");
    if (max_age != std::chrono::seconds::zero()) {
      cookie_value.append("; Max-Age=").append(std::to_string(max_age.count()));
    }
    if (!domain.empty()) {
      cookie_value.append("; Domain=").append(domain);
    }
    if (!path.empty()) {
      cookie_value.append("; Path=").append(path);
    }
    if (httponly) {
      cookie_value.append("; HttpOnly");
    }
    return cookie_value;
  }

  /**
   * 从url中解析查询参数
   * @param url
   * @return std::unordered_map<std::string, std::string> key为参数名,value为参数值
   */
  static std::unordered_map<std::string, std::string> parseQueryParams(std::string_view url) {
    size_t start = url.find('?');
    if (start == std::string_view::npos) {
      return std::unordered_map<std::string, std::string>();
    }

    start++;
    return parseParameters(url, start, true);
  }

  /**
   * 从body中解析查询参数
   * @param body 表单类型的body
   * @return std::unordered_map<std::string, std::string> key为参数名,value为参数值
   */
  static std::unordered_map<std::string, std::string> parseFromBody(std::string_view body) {
    return parseParameters(body, 0, true);
  }

  static std::string_view stripQueryString(std::string_view url) {
    size_t query_offset = url.find('?');
    return std::string_view(url.data(), query_offset != url.npos ? query_offset : url.size());
  }

  /**
   * 将url编码格式的字符串解码
   * @param encoded url编码参数
   * @return std::string
   */
  static std::string percentDecoding(std::string_view encoded) {
    std::string decoded;
    decoded.reserve(encoded.size());
    for (size_t i = 0; i < encoded.size(); ++i) {
      char ch = encoded[i];
      if (ch == '%' && i + 2 < encoded.size()) {
        const char& hi = encoded[i + 1];
        const char& lo = encoded[i + 2];
        if (ascii_isdigit(hi)) {
          ch = hi - '0';
        } else {
          ch = ascii_toupper(hi) - 'A' + 10;
        }

        ch *= 16;
        if (ascii_isdigit(lo)) {
          ch += lo - '0';
        } else {
          ch += ascii_toupper(lo) - 'A' + 10;
        }
        i += 2;
      }
      decoded.push_back(ch);
    }
    return decoded;
  }

private:
  static std::unordered_map<std::string, std::string>
  parseParameters(std::string_view data, size_t start, bool decode_params) {
    std::unordered_map<std::string, std::string> params;

    while (start < data.size()) {
      size_t end = data.find('&', start);
      if (end == std::string::npos) {
        end = data.size();
      }
      std::string_view param(data.data() + start, end - start);

      const size_t equal = param.find('=');
      if (equal != std::string::npos) {
        const auto param_name = StringUtil::subspan(data, start, start + equal);
        const auto param_value = StringUtil::subspan(data, start + equal + 1, end);
        params.emplace(decode_params ? Url::unmarshal(param_name) : param_name,
                       decode_params ? Url::unmarshal(param_value) : param_value);
      } else {
        params.emplace(StringUtil::subspan(data, start, end), "");
      }

      start = end + 1;
    }

    return params;
  }
  static char ascii_toupper(unsigned char c) { return kToUpper[c]; }
  static bool ascii_isdigit(unsigned char c) { return c >= '0' && c <= '9'; }
};

} // namespace Utility
} // namespace v1_2_x
} // namespace SrhinoPluginFramework