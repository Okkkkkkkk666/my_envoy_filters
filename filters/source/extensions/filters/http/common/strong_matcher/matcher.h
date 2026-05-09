#pragma once

#include "source/common/network/cidr_range.h"
#include "source/common/common/matchers.h"
#include "source/common/http/header_utility.h"
#include "source/common/router/config_utility.h"
#include "filters/api/envoy/extensions/filters/http/common/strong_matcher/v3/matcher.pb.h"
#include "query_parameter_matcher.h"
#include "user_matcher.h"
#include "ip_list.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace StrongMatcher {

class Matcher {
public:
  Matcher(const v3::Matcher& matcher);

public:
  /**
   * 路径匹配
   * @param headers
   * @return true
   * @return false
   */
  bool matchPath(const Http::RequestHeaderMap& headers) const {
    if (path_matcher_ != nullptr) {
      bool is_match = false;
      if (path_matcher_->match(headers.getPathValue())) {
        is_match = true;
      }

      if (path_invert_) {
        is_match = !is_match;
      }

      if (!is_match) {
        return false;
      }
    }
    return true;
  }

  /**
   * HTTP头匹配
   * @param headers
   * @return true
   * @return false
   */
  bool matchHeader(const Http::HeaderMap& headers) const {
    return Http::HeaderUtility::matchHeaders(headers, header_matchers_);
  }

  /**
   * 查询参数匹配
   * @param headers
   * @return true
   * @return false
   */
  bool matchQueryParameter(const Http::RequestHeaderMap& headers) const {
    Http::Utility::QueryParams query_parameters =
        Http::Utility::parseQueryString(headers.getPathValue());
    for (const auto& query_parameter_matcher : query_parameter_matchers_) {
      if (!query_parameter_matcher->matches(query_parameters)) {
        return false;
      }
    }
    return true;
  }

  /**
   * HTTP方法名匹配
   * @param headers
   * @return true
   * @return false
   */
  bool matchMethods(const Http::RequestHeaderMap& headers) const {
    if (!methods_.empty()) {
      absl::string_view method_value_view = headers.getMethodValue();
      std::string method_value(method_value_view.data(), method_value_view.size());
      std::transform(method_value.begin(), method_value.end(), method_value.begin(), ::toupper);
      bool is_match = false;
      for (const auto& method : methods_) {
        if (method == method_value) {
          is_match = true;
          break;
        }
      }

      if (method_invert_) {
        is_match = !is_match;
      }

      if (!is_match) {
        return false;
      }
    }
    return true;
  }

  /**
   * 用户名匹配
   * @param headers
   * @return true
   * @return false
   */
  bool matchUser(const std::string& username) const {
    for (const auto& user_matcher : user_matchers_) {
      if (!user_matcher->matches(username)) {
        return false;
      }
    }
    return true;
  }

  /**
   * Ip匹配
   * @param address
   * @return true
   * @return false
   */
  bool matchIp(const Network::Address::InstanceConstSharedPtr& address) const {
    bool is_match = false;
    if (ip_range_.empty() && ip_list_ == nullptr) {
      return true;
    }
   // IpRange匹配
    if (!ip_range_.empty()) {
      for (const auto& ip_range : ip_range_) {
        if (ip_range->isInRange(*address.get())) {
          is_match = true;
          break;
        }
      }
    }
    // ipset匹配
    if (!is_match) {
      if (ip_list_ != nullptr) {
        if (ip_list_->isInRange(*address.get())) {
          is_match = true;
        }
      }
    }

    if (ip_invert_) {
      is_match = !is_match;
    }

    return is_match;
  }

  /**
   * 路径、HTTP头、查询参数、HTTP方法名等同时匹配
   * @param headers
   * @return true
   * @return false
   */
  bool matchAll(const Http::RequestHeaderMap& headers) const {
    return matchPath(headers) && matchHeader(headers) && matchQueryParameter(headers) &&
           matchMethods(headers);
  }

  /**
   * matchALL、IP范围、IP、用户名等同时匹配
   * @param headers
   * @return true
   * @return false
   */
  bool matchAll(const Http::RequestHeaderMap& headers,
                const Network::Address::InstanceConstSharedPtr& address,
                const std::string& username) const {
    return matchAll(headers) && matchIp(address) && matchUser(username);
  }

  const std::vector<UserMatcherptr>& getUserMatchers() const { return user_matchers_; }

  /**
   * 获取hash值
   * @return uint64_t
   */
  uint64_t hash() const { return hash_code_; }

  // 计算hash
public:
  static void hashUpdate(const void* data, size_t len, uint64_t& hash_code) {
    const unsigned char* data_byte = reinterpret_cast<const unsigned char*>(data);
    for (size_t i = 0; i < len; ++i) {
      hash_code = hash_code * 31 + data_byte[i];
    }
  }

  template <typename T> static void hashUpdate(T&& v, uint64_t& hash_code) {
    hashUpdate(&v, sizeof(T), hash_code);
  }

private:
  Matchers::PathMatcherConstSharedPtr path_matcher_;
  std::vector<Http::HeaderUtility::HeaderDataPtr> header_matchers_;
  std::vector<QueryParameterMatcherPtr> query_parameter_matchers_;
  std::vector<std::string> methods_;
  std::vector<UserMatcherptr> user_matchers_;
  std::vector<IpRangePtr> ip_range_;
  IpSetPtr ip_list_;
  bool method_invert_;
  bool ip_invert_;
  bool path_invert_;
  uint64_t hash_code_;

  // 构造Contains
private:
  static Matchers::PathMatcherConstSharedPtr createContainsPath(const std::string& contains,
                                                                bool ignore_case);

private:
  Matchers::PathMatcherConstSharedPtr makePathMatcher(const v3::Matcher& matcher) const;
  std::vector<QueryParameterMatcherPtr>
  buildQueryParameterMatcherVector(const v3::Matcher& matcher) const;
  std::vector<std::string> buildMethodsVector(const v3::Matcher& matcher) const;
  std::vector<UserMatcherptr> buildUserMatcherVector(const v3::Matcher& matcher) const;
  std::vector<IpRangePtr> buildIpRangeVector(const v3::Matcher& matcher) const;
  IpSetPtr buildIpSet(const v3::Matcher& matcher) const;

  // 计算hash
private:
  static uint64_t calcHash(const v3::Matcher& matcher);
  static void updateHashWithPath(const v3::Matcher& matcher, uint64_t& hash_code);
  static void updateHashWithHeader(const v3::Matcher& matcher, uint64_t& hash_code);
  static void updateHashWithQureyParameter(const v3::Matcher& matcher, uint64_t& hash_code);
  static void updateHashWithMethods(const v3::Matcher& matcher, uint64_t& hash_code);
  static void updateHashWithStringMatcher(const envoy::type::matcher::v3::StringMatcher& matcher,
                                          uint64_t& hash_code);
  static void updateHashWithUserMatcher(const v3::Matcher& matcher, uint64_t& hash_code);
  static void updateHashWithIpSet(const v3::Matcher& matcher, uint64_t& hash_code);
  static void updateHashWithIpRange(const v3::Matcher& matcher, uint64_t& hash_code);
};

} // namespace StrongMatcher
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
