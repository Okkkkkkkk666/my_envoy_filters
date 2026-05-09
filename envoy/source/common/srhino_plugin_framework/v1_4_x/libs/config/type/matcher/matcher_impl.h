#pragma once

#include <memory>
#include "query_parameter_matcher_impl.h"
#include "user_matcher_impl.h"
#include "ip_list_impl.h"
#include "region_matcher_impl.h"
#include "date_matcher_impl.h"
#include "source/common/common/matchers.h"
#include "source/common/http/header_utility.h"
#include "source/common/router/config_utility.h"
#include "source/common/network/address_impl.h"
#include "envoy/srhino_plugin_framework/v1_4_x/libs/config/type/matcher/matcher.h"
namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Config {
namespace Type {
namespace Matcher {
namespace v3 = srhino_plugin_framework::v1_4_x::proto::config::type::matcher;
class MatcherImpl : public Matcher {
public:
  MatcherImpl(
      const srhino_plugin_framework::v1_4_x::proto::config::type::matcher::Matcher& matcher);

public:
  /**
   * 路径匹配
   * @param headers
   * @return true
   * @return false
   */
  bool matchPath(const HeaderContext& context) const override {
    const Envoy::Http::RequestHeaderMap* headers = convertProtoConfig(context.headers());
    if (!headers) {
      return false;
    }

    // 匹配路径白名单
    const std::string path = (*headers).getPathValue().data();
    if (exclude_path_.find(path) != exclude_path_.end()) {
      return false;
    }

    if (path_matcher_ != nullptr) {
      bool is_match = false;
      if (path_matcher_->match((*headers).getPathValue())) {
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
  bool matchHeader(const HeaderContext& context) const override {
    const Envoy::Http::RequestHeaderMap* headers = convertProtoConfig(context.headers());
    if (!headers) {
      return false;
    }
    return Envoy::Http::HeaderUtility::matchHeaders(*headers, header_matchers_);
  }

  /**
   * 查询参数匹配
   * @param headers
   * @return true
   * @return false
   */
  bool matchQueryParameter(const HeaderContext& context) const override {
    const Envoy::Http::RequestHeaderMap* headers = convertProtoConfig(context.headers());
    if (!headers) {
      return false;
    }
    Envoy::Http::Utility::QueryParams query_parameters =
        Envoy::Http::Utility::parseQueryString((*headers).getPathValue());
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
  bool matchMethods(const HeaderContext& context) const override {
    const Envoy::Http::RequestHeaderMap* headers = convertProtoConfig(context.headers());
    if (!headers) {
      return false;
    }
    if (!methods_.empty()) {
      absl::string_view method_value_view = (*headers).getMethodValue();
      std::string method_value(method_value_view.data(), method_value_view.size());
      std::transform(method_value.begin(), method_value.end(), method_value.begin(), ::toupper);
      bool is_match = false;
      
      if (methods_.find(method_value) != methods_.end()) {
        is_match = true;
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
  bool matchUser(const std::string& username) const override {
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
  bool matchIp(uint32_t address) const override {

    bool is_match = false;
    if (ip_range_.empty() && ip_list_ == nullptr && ip_groups_.empty()) {
      return true;
    }
    // IpRange匹配
    if (!ip_range_.empty()) {
      for (const auto& ip_range : ip_range_) {
        if (ip_range->isInRange(address)) {
          is_match = true;
          break;
        }
      }
    }
    // ipset匹配
    if (!is_match) {
      if (ip_list_ != nullptr) {
        if (ip_list_->isInRange(address)) {
          is_match = true;
        }
      }
    }
    // ip group匹配
    if (!ip_groups_.empty()) {
      auto invert = ip_groups_.front()->invert();
      for (const auto& ip_group : ip_groups_) {
        if (ip_group->isInRange(address)) {
          is_match = true;
          break;
        }
      }
      if (invert) {
        is_match = !is_match;
      }
    }

    if (ip_invert_) {
      is_match = !is_match;
    }

    return is_match;
  }

  /**
   * Ip归属地匹配
   * @param region_name
   * @return true
   * @return false
   */
  bool matchIpRegion(const std::string& region_name) const override {
    if (region_matcher_ == nullptr) {
      return true;
    }

    if (region_matcher_->match(region_name)) {
      return true;
    }
    return false;
  }

  /**
   * referer匹配
   * @param context
   * @return true
   * @return false
   */
  bool matchReferer(const HeaderContext& context) const override {
    const Envoy::Http::RequestHeaderMap* headers = convertProtoConfig(context.headers());
    if (!headers) {
      return false;
    }
    return Envoy::Http::HeaderUtility::matchHeaders(*headers, referer_matchers_);
  }

  /**
   * Date匹配
   * @return true
   * @return false
   */
  bool matchTime() const override {
    if (date_matcher_ == nullptr) {
      return true;
    }
    if (date_matcher_->match()) {
      return true;
    }
    return false;
  }

  /**
   * 路径、HTTP头、查询参数、HTTP方法名等同时匹配
   * @param context
   * @return true
   * @return false
   */
  bool matchAll(const HeaderContext& context) const override {
    return matchPath(context) && matchHeader(context) && matchQueryParameter(context) &&
           matchMethods(context);
  }

  /**
   * matchALL、IP范围、IP、用户名等同时匹配
   * @param context
   * @param address
   * @param username
   * @return true
   * @return false
   */
  bool matchAll(const HeaderContext& context, uint32_t address, const std::string& username) const {

    return matchAll(context) && matchIp(address) && matchUser(username);
  }

  /**
   * 路径、HTTP头、查询参数、HTTP方法名、用户名、IP、IP归属地、referer、时间等同时匹配
   * @param context
   * @param username
   * @param address
   * @param region_name
   * @return true
   * @return false
   */
  bool matchAll(const HeaderContext& context, uint32_t address, const std::string& username,
                const std::string& region_name) const override {
    return matchAll(context, address, username) && matchIpRegion(region_name) &&
           matchReferer(context) && matchTime();
  }

  bool getUserMatchers() const override {
    if (!user_matchers_.empty()) {
      return true;
    }
    return false;
  }

  /**
   * 获取hash值
   * @return uint64_t
   */
  uint64_t hash() const override { return hash_code_; }

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
  Envoy::Matchers::PathMatcherConstSharedPtr path_matcher_;
  std::vector<Envoy::Http::HeaderUtility::HeaderDataPtr> header_matchers_;
  std::vector<Envoy::Http::HeaderUtility::HeaderDataPtr> referer_matchers_;
  std::vector<QueryParameterMatcherPtr> query_parameter_matchers_;
  std::unordered_map<std::string, std::string> methods_;
  std::vector<UserMatcherptr> user_matchers_;
  std::vector<IpRangePtr> ip_range_;
  std::vector<IPGroupsPtr> ip_groups_;
  IpSetPtr ip_list_;
  bool method_invert_;
  bool ip_invert_;
  bool path_invert_;
  uint64_t hash_code_;
  RegionMatcherptr region_matcher_;
  std::unordered_map<std::string, uint32_t> exclude_path_;
  DateMatcherptr date_matcher_;

  // 构造Contains
private:
  static Envoy::Matchers::PathMatcherConstSharedPtr createContainsPath(const std::string& contains,
                                                                       bool ignore_case);

private:
  Envoy::Matchers::PathMatcherConstSharedPtr makePathMatcher(const v3::Matcher& matcher) const;
  std::vector<QueryParameterMatcherPtr>
  buildQueryParameterMatcherVector(const v3::Matcher& matcher) const;
  std::unordered_map<std::string, std::string> buildMethodsVector(const v3::Matcher& matcher) const;
  std::vector<UserMatcherptr> buildUserMatcherVector(const v3::Matcher& matcher) const;
  std::vector<IpRangePtr> buildIpRangeVector(const v3::Matcher& matcher) const;
  IpSetPtr buildIpSet(const v3::Matcher& matcher) const;
  std::vector<IPGroupsPtr> buildIpGroups(const v3::Matcher& matcher) const;
  RegionMatcherptr buildRegionMatcher(const v3::Matcher& matcher) const;
  std::unordered_map<std::string, uint32_t> buildExcludePath(const v3::Matcher& matcher) const;
  DateMatcherptr buildDateMatcher(const v3::Matcher& matcher) const;

private:
  const google::protobuf::RepeatedPtrField<envoy::config::route::v3::HeaderMatcher>
  convertProtoConfig(const google::protobuf::RepeatedPtrField<
                     srhino_plugin_framework::v1_4_x::proto::config::type::matcher::HeaderMatcher>&
                         HeaderMatcher) const;

  const Envoy::Http::RequestHeaderMap*
  convertProtoConfig(const SrhinoPluginFramework::v1_4_x::HeaderMap& header_map) const;
  // 计算hash
private:
  static uint64_t calcHash(const v3::Matcher& matcher);
  static void updateHashWithPath(const v3::Matcher& matcher, uint64_t& hash_code);
  static void updateHashWithHeader(const v3::Matcher& matcher, uint64_t& hash_code);
  static void updateHashWithQureyParameter(const v3::Matcher& matcher, uint64_t& hash_code);
  static void updateHashWithMethods(const v3::Matcher& matcher, uint64_t& hash_code);
  static void updateHashWithStringMatcher(
      const srhino_plugin_framework::v1_4_x::proto::config::type::matcher::StringMatcher& matcher,
      uint64_t& hash_code);
  static void updateHashWithUserMatcher(const v3::Matcher& matcher, uint64_t& hash_code);
  static void updateHashWithIpSet(const v3::Matcher& matcher, uint64_t& hash_code);
  static void updateHashWithIpRange(const v3::Matcher& matcher, uint64_t& hash_code);
};
} // namespace Matcher
} // namespace Type
} // namespace Config
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework