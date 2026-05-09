#pragma once
#include <iostream>
#include <iomanip>
#include <regex>
#include <memory>
#include <string>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <vector>
#include <stdexcept>
#include "absl/strings/str_split.h"

#include "source/common/common/logger.h"
#include "source/common/http/headers.h"
#include "envoy/network/address.h"
#include "envoy/http/header_map.h"
#include "filters/source/extensions/filters/http/common/regex_matcher/regex_matcher.h"
#include "filters/api/envoy/extensions/filters/http/user_identify/v3/user_identify.pb.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace UserIdentify {
namespace Impl {

namespace v3 = envoy::extensions::filters::http::user_identify::v3;
namespace RegexMatcher = Envoy::Extensions::Filters::Common::RegexMatcher;
using RegexMatcherPtr = std::shared_ptr<RegexMatcher::RegexMatcher>;


enum ContentType {
  CONTENT_TYPE_FROM_URLENCODED,
  CONTENT_TYPE_JSON,
  CONTENT_TYPE_XML,
  CONTENT_TYPE_UNKNOWN,
  CONTENT_TYPE_MAX = CONTENT_TYPE_UNKNOWN
};

// token 的提取正则表达式类型
enum TokenRegexType {
  TOKEN_TYPE_JSON,
  TOKEN_TYPE_COOKIE,
  TOKEN_TYPE_UNKNOWN,
  TOKEN_TYPE_MAX = TOKEN_TYPE_UNKNOWN
};

// http 类型
enum HttpType {
  HTTP_TYPE_LOGIN,
  HTTP_TYPE_GUEST,
  HTTP_TYPE_UNKNOWN,
  HTTP_TYPE_MAX = HTTP_TYPE_UNKNOWN
};

struct RegexBlock {
  std::string regex_str; // 原始正则表达式
  size_t      extract_group; // 要提取字符串所在group
  std::regex  re;        // 编译后的正则表达式
};
using RegexGroup = std::vector<RegexBlock>;

struct IdentifyCtx {
  int thread_index;
  int rule_index; // used by Customize
};
using IdentifyCtxPtr = std::shared_ptr<struct IdentifyCtx>;

template<class T> ContentType getContentType(const T& content_type) {
  std::string str(content_type.data(), content_type.length());
  std::transform(str.begin(), str.end(), str.begin(), ::tolower);

  if (str.find("x-www-form-urlencoded") != std::string::npos) {
    return ContentType::CONTENT_TYPE_FROM_URLENCODED;
  } else if (str.find("json") != std::string::npos) {
    return ContentType::CONTENT_TYPE_JSON;
  } else if (str.find("xml") != std::string::npos
            || str.find("html") != std::string::npos) {
    return ContentType::CONTENT_TYPE_XML;
  }
  return ContentType::CONTENT_TYPE_UNKNOWN;
}

template<class T> bool isContentCompressed(const T& headers) {
  Envoy::Http::HeaderMap::GetResult get_result =
      headers.get(Http::CustomHeaders::get().ContentEncoding);
  if (!get_result.empty()) {
    auto content_encoding = get_result[0]->value().getStringView();
    std::string str(content_encoding.data(), content_encoding.length());
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    if (str.find("gzip") != std::string::npos) {
      return true;
    } else if (str.find("br") != std::string::npos) {
      return true;
    }
  }
  return false;
}

struct MatcherBlock {
  std::vector<RegexMatcherPtr> regex_matchers_; // 用于查找到匹配位置
  std::vector<RegexGroup> regex_groups_; // 用于提取匹配到的内容
};

using MatcherBlockPtr = std::shared_ptr<MatcherBlock>;

/**
 * 用户识别基类，实现token的获取，登陆请求判断功能
 * 该类是无状态的
 */
class IdentifyBase : public Logger::Loggable<Logger::Id::filter> {
public:
  IdentifyBase(const std::string& url_list, const std::string& login_token_list, const std::string& guest_token_list);
  virtual ~IdentifyBase() {};

public:
  /**
   * 上下文初始化
   * 继承类首次使用前，先调用这个函数初始化一个ctx
  */
  virtual bool initCtx(const std::string& cluster_name, int thread_index, IdentifyCtxPtr ctx) = 0;

  /**
   * @param(in)
   *  url 要判断的url
   * @param(in)
   *  ctx
   * @return
   *  true 是登陆请求
   *  false 不是
  */
  virtual bool isLogin(const std::string_view& url, const IdentifyCtxPtr ctx) = 0;

  /**
   * 从url中获取用户名
  */
  virtual bool getUserNameFromUrl(const std::string_view& url, const IdentifyCtxPtr ctx, std::string& user_name) = 0;
  /**
   * 从内容数据中获取用户名，要指定内容的格式
  */
  virtual bool getUserNameFromBody(const std::string_view& data, const IdentifyCtxPtr ctx, ContentType type, std::string& user_name) = 0;

public:
  /**
   * 使用内部定义url判断是否是登陆请求
  */
  virtual bool isLoginByDefault(const std::string_view& url, const IdentifyCtxPtr ctx);
  /**
   * 从cookie获取token
  */
  bool getTokenFromCookie(const std::string_view& data, const IdentifyCtxPtr ctx, HttpType http_type, std::string& token) const;
  /**
   * 从内容数据中获取token，要指定内容的格式
  */
  bool getTokenFromBody(const std::string_view& data, const IdentifyCtxPtr ctx, HttpType http_type, TokenRegexType token_type, std::string& token) const;

public:
  static RegexMatcherPtr regexMatcherInit(const std::vector<RegexMatcher::ExpressionView>& exprs);
  static RegexMatcherPtr regexMatcherInit(const std::vector<std::string>& regex_exprs);
  static RegexMatcherPtr regexMatcherInit(const RegexGroup& regex_group);

private:
  bool realGetToken(const RegexGroup& regex_group, const std::string_view& data_view, size_t id, std::string& token) const;
  bool getTokenByType(const std::string_view& data_view, const IdentifyCtxPtr ctx, HttpType http_type, TokenRegexType token_type, std::string& token) const;
  MatcherBlockPtr initMatcherBlock(const std::string& matcher_name, const std::vector<std::string>& keys, int group_size);

private:
  const std::vector<std::string> urls_;
  RegexMatcherPtr url_regex_matcher_; //用于匹配url

  // token可位于2个位置： http header的cookie或者set_cookie中; http body中，一般为json格式
  const std::vector<std::string> login_tokens_;
  const std::vector<std::string> guest_tokens_;
  std::vector<MatcherBlockPtr> token_matchers_;
};

class CustomRule : public Logger::Loggable<Logger::Id::filter> {
public:
  CustomRule(const v3::Customization& customization, size_t id);
  inline bool enabled() const { return enabled_; }
  inline const std::vector<std::string>& cluster_name() const { return cluster_name_; }
  inline const std::vector<std::string>& urls() const { return urls_; }
  inline const std::vector<std::string>& user_name() const { return user_name_; }
  inline const std::string& common_info_name() const { return common_info_name_; }
  inline size_t id() { return id_; }
public:
  /**
   * @param(in)
   *  url 要配置的url，应为小写
   * @return
   *  bool true 配置成功 false 失败
  */
  bool matchUrl(const std::string_view& url) const;
  bool getUserNameByType(const std::string_view& data_view, ContentType type, int thread_index, std::string& user_name);
  bool realGetUserName(const std::string_view& data_view, ContentType type, size_t id, std::string& user_name);
private:
  const bool enabled_;
  const std::vector<std::string> cluster_name_;
  const std::vector<std::string> urls_;
  const std::vector<std::string> user_name_;
  const std::string common_info_name_;
  const size_t id_;
  const std::string url_match_str_;

  // 下面两个结构成对使用
  std::vector<RegexMatcherPtr> user_name_regex_matchers_; // 用于查找到匹配位置
  std::vector<RegexGroup> user_name_regex_groups_; // 用于提取匹配到的内容
};
using CustomRulePtr = std::shared_ptr<CustomRule>;

class Customize : public IdentifyBase {
public:
  Customize(const v3::Customize& customize, const std::string& url_list, const std::string& login_token_list, const std::string& guest_token_list);

public:
  bool initCtx(const std::string& cluster_name, int thread_index, IdentifyCtxPtr ctx);
  bool isLogin(const std::string_view& url, const IdentifyCtxPtr ctx);
  bool getUserNameFromUrl(const std::string_view& url, const IdentifyCtxPtr ctx, std::string& user_name);
  bool getUserNameFromBody(const std::string_view& data, const IdentifyCtxPtr ctx, ContentType type, std::string& user_name);

private:
  std::vector<CustomRulePtr> rules_;
  std::map<std::string, int> cluster_map_;
};

class Auto : public IdentifyBase{
public:
  Auto(const std::string& user_name_list, const std::string& url_list, const std::string& login_token_list, const std::string& guest_token_list);
public:
  bool isLogin(const std::string_view& url, const IdentifyCtxPtr ctx);
  bool initCtx(const std::string& cluster_name, int thread_index, IdentifyCtxPtr ctx);
  bool getUserNameFromUrl(const std::string_view& url, const IdentifyCtxPtr ctx, std::string& user_name);
  bool getUserNameFromBody(const std::string_view& data, const IdentifyCtxPtr ctx, ContentType type, std::string& user_name);
private:
  bool getUserNameByType(const std::string_view& data_view, ContentType type, int thread_index, std::string& user_name);

private:
  const std::vector<std::string> user_name_list_;
  // 下面两个结构成对使用
  std::vector<RegexMatcherPtr> user_name_regex_matchers_; // 用于查找到匹配位置
  std::vector<RegexGroup> user_name_regex_groups_; // 用于提取匹配到的内容
};

} // namespace Impl
} // namespace UserIdentify
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy