#pragma once
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <algorithm>
#include <sstream>
#include "third-party/Cryptopp/sha.h"
#include "third-party/Cryptopp/md5.h"
#include "third-party/Cryptopp/hex.h"
#include "third-party/Cryptopp/base64.h"
#include "source/common/common/logger.h"
#include "source/common/common/base64.h"
#include "source/common/http/headers.h"
#include "filters/source/extensions/filters/http/common/regex_matcher/regex_matcher.h"
#include "filters/api/envoy/extensions/filters/http/weak_password_check/v3/weak_password.pb.h"
#include "custom_password.h"
#include "database.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WeakPasswordCheck {
namespace Impl {

namespace RegexMatcher = Envoy::Extensions::Filters::Common::RegexMatcher;
namespace v3 = envoy::extensions::filters::http::weak_password_check::v3;

enum EncryptType {
  ENCRYPT_TYPE_MD5,
  ENCRYPT_TYPE_SHA1,
  ENCRYPT_TYPE_SHA256,
  ENCRYPT_TYPE_SHA512,
  ENCRYPT_TYPE_SHA3,
  ENCRYPT_TYPE_SM3,
  ENCRYPT_TYPE_MAX,
};

enum PossibleEncryptType {
  POSSIBLE_ENCRYPT_TYPE_SHA1,
  POSSIBLE_ENCRYPT_TYPE_MD5_SHA256_SM3,
  POSSIBLE_ENCRYPT_TYPE_SHA512_SHA3,
  POSSIBLE_ENCRYPT_TYPE_UNKNOWN,
  POSSIBLE_ENCRYPT_TYPE_MAX = POSSIBLE_ENCRYPT_TYPE_UNKNOWN
};

enum TrigRuleType {
  TrigNone = 0,
  TrigInternal = 1,
  TrigCustom = 2,
};

class WpdUtility : public Logger::Loggable<Logger::Id::filter> {
public:
  WpdUtility() = delete;
public:
  static std::string MD5(const std::string& data);
  static std::string SHA256(const std::string& data);
  static std::string SHA1(const std::string& data);
  /**
   * 判断是否为明文密码
  */
  static PossibleEncryptType getPossibleEncryptType(const std::string& password);
  static bool isBase64(const std::string& data);
  /**
   * 尝试解base64编码，判断解码后是否为可见字符
   * @param data base64编码字符串
   * @param decoded_data 输出参数，返回解base64后的字符串
   * @return true： 解码后为都是可见字符的字符串，false：解码后含有非可见字符
   */
  static bool tryDecodeBase64(const std::string& data, std::string& decoded_data);
  static bool isPasswordWeak(const std::string& user_name, const std::string& password);
  static bool isNameReverse(const std::string& user_data, const std::string& password_data);
  static bool isIgnoreResourcesRequest(const std::string_view& path);
  static std::string& deSensitive(std::string& password);
  static uint64_t hexStringToUint64(const std::string& hex_string);
  static uint64_t getKeyByPossibleEncryptType(const std::string& encrypt_password, enum PossibleEncryptType type);
  static uint64_t getKeyByEncryptType(const std::string& encrypt_password, enum EncryptType type);

public:
  static const int WeakPasswordLength{6};

};

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

enum FieldType {
  FIELD_TYPE_USER_NAME,
  FIELD_TYPE_PASSWORD_NAME,
};

struct RegexBlock {
  std::string regex_str; // 原始正则表达式
  size_t      extract_group; // 要提取字符串所在group
  std::regex  re;        // 编译后的正则表达式
  uint32_t    type;      // 类型，用于指明当前正则所属类型
};
using RegexGroup = std::vector<RegexBlock>;

using RegexMatcherPtr = std::shared_ptr<RegexMatcher::RegexMatcher>;
struct MatcherBlock {
  std::vector<RegexMatcherPtr> regex_matchers_; // 用于查找到匹配位置
  std::vector<RegexGroup> regex_groups_; // 用于提取匹配到的内容
};
using MatcherBlockPtr = std::shared_ptr<MatcherBlock>;

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

//TODO local cache for sqlite query
class AutoConfig : public Logger::Loggable<Logger::Id::filter> {
public:
  AutoConfig(const v3::WeakPasswordCheckGlobal& proto_config);
  virtual ~AutoConfig();

public:
  /**
   * 提取用户名，密码字段
   * @param data_view
   *    要匹配的数据
   * @param type
   *    data_view的格式， form、json、xml...
   * @param thread_index
   *    当前线程序号，hyperscan需要
   * @param user_info
   *    输出参数， 存放<user_name, password>对
  */
  bool doExtractUserInfo(const std::string_view& data_view, enum ContentType type,
                               int thread_index,
                               std::vector<std::pair<std::string, std::string>>& user_info);
  bool realExtractUserInfo(RegexMatcherPtr regex_matcher, RegexGroup& regex_group,
                           const std::string_view& data_view, int thread_index,
                           std::vector<std::pair<std::string, std::string>>& user_info);
  virtual bool extractUserInfo(const std::string& cluster_name, const std::string_view& data_view,
                       enum ContentType type, int thread_index,
                       std::vector<std::pair<std::string, std::string>>& user_info);
  virtual bool isLoginSuccess(const std::string_view& data_view, enum TokenRegexType type, int thread_index);
  /**
   * 获取密文的明文
   * 1. 查询本地缓存
   * 2. 查询数据库
   * 
   * @param key
   *    密文key
   * @param plain_password
   *    查询得到的明文
   * @return
   *    true 查询成功， 是弱密码
   *    false 查询失败， 是强密码
  */
  bool queryPlainPassword(const std::string& password, enum PossibleEncryptType type, std::string& plain_password, enum TrigRuleType& trig_type);

  static MatcherBlockPtr initTokenMatcherBlock(const std::string& matcher_name,
                                        const std::vector<std::string>& token_names,
                                        int group_size,
                                        const std::string& db_path = std::string());
  static MatcherBlockPtr initMatcherBlock(const std::string& matcher_name,
                                          const std::vector<std::string>& user_names,
                                          const std::vector<std::string>& password_names,
                                          int group_size,
                                          const std::string& db_path = std::string());
  static RegexMatcherPtr regexMatcherInit(const std::vector<RegexMatcher::ExpressionView>& exprs,
                                          const std::string& db_path = std::string());
  // static RegexMatcherPtr regexMatcherInit(const std::vector<std::string>& regex_exprs);
  static RegexMatcherPtr regexMatcherInit(const RegexGroup& regex_group,
                                          const std::string& db_path = std::string());

private:
  bool splitUserInfoMatchResults(const std::vector<RegexMatcher::MatchResult>& results,
                                 const RegexGroup& regex_group,
                                 std::vector<RegexMatcher::MatchResult>& user_name_results,
                                 std::vector<RegexMatcher::MatchResult>& password_name_results);
  bool realExtract(const std::string_view& data_view, const RegexGroup& group,
                   const RegexMatcher::MatchResult& result, std::string& str);
  void migrate();

public:
  static const int Md5Length{32};

private:
  CustomPasswordPtr custom_password_{nullptr};
  RainbowDbPtr rainbow_{nullptr};

  // 由于user_info, token很少改变，且数量比较大，编译时间很长，因此使用hyperscan的预编译功能
  static const std::string user_info_matchers_db_path_;
  static const std::string token_matchers_db_path_;
  // 将用户名和密码名放入matcher中，一起匹配
  MatcherBlockPtr user_info_matchers_;
  MatcherBlockPtr token_matchers_;
};
using AutoConfigPtr = std::shared_ptr<AutoConfig>;

class CustomRule : public Logger::Loggable<Logger::Id::filter> {
public:
  CustomRule(const v3::AdvanceConfig& advance_config);
  inline const std::vector<std::string>& cluster_name() const { return cluster_name_; }
  inline const MatcherBlockPtr user_info_matchers() const { return user_info_matchers_; }
private:
  const std::vector<std::string> cluster_name_;
  MatcherBlockPtr user_info_matchers_{nullptr};
};
using CustomRulePtr = std::shared_ptr<CustomRule>;


class AdvancedConfig : public AutoConfig {
public:
  AdvancedConfig(const v3::WeakPasswordCheckGlobal& proto_config);
  virtual ~AdvancedConfig() {}

  bool extractUserInfo(const std::string& cluster_name, const std::string_view& data_view,
                       enum ContentType type, int thread_index,
                       std::vector<std::pair<std::string, std::string>>& user_info) override;
private:
  bool doExtractUserInfo(const CustomRulePtr rule, const std::string_view& data_view,
                         enum ContentType type, int thread_index,
                         std::vector<std::pair<std::string, std::string>>& user_info);

private:
  std::vector<CustomRulePtr> rules_;
  std::map<std::string, CustomRulePtr> cluster_map_;
};

} // namespace Impl
} // namespace WeakPasswordCheck
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy