#pragma once

#include <vector>
#include <map>
#include <iostream>
#include <sstream>
#include <ctime>
#include <queue>
#include <algorithm>
#include "source/extensions/filters/http/common/pass_through_filter_ex.h"
#include "source/common/router/config_impl.h"
#include "source/common/buffer/buffer_impl.h"

#include "filters/api/envoy/extensions/filters/http/response_rewrite/v3/response_rewrite.pb.h"
#include "filters/api/envoy/extensions/filters/http/response_rewrite/v3/response_rewrite_log.pb.h"
#include "filters/source/extensions/filters/http/common/regex_matcher/regex_matcher.h"

#include "filters/source/extensions/filters/http/common/strong_matcher/matcher.h"
#include "filters/source/extensions/filters/http/common/strong_matcher/rule.h"
#include "filters/source/extensions/filters/http/common/ip_whitelist/ip_whitelist.h"
#include "filters/source/extensions/filters/http/common/strong_matcher/user_matcher.h"

#include "filters/source/extensions/filters/http/common/algorithm/cover.h"
#include "filters/source/extensions/filters/http/common/algorithm/encrypt_algorithm.h"
#include "filters/source/extensions/filters/http/common/algorithm/hash_encrypt.h"
#include "filters/source/extensions/filters/http/common/algorithm/replace.h"
#include "filters/source/extensions/filters/http/common/algorithm/shuffle.h"
#include "filters/source/extensions/filters/http/common/algorithm/transform.h"

#include "envoy/compression/decompressor/config.h"
#include "envoy/compression/decompressor/decompressor.h"
#include "envoy/compression/compressor/config.h"
#include "envoy/compression/compressor/compressor.h"

#define FILTER_NAME "envoy.filters.http.response-rewrite.1.0"
const std::string SrhinoKeyUserName("user_name");
const std::string SrhinoDomainMetaData("srhino_metadata");
namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ResponseRewrite {

namespace v3 = envoy::extensions::filters::http::response_rewrite::v3;
namespace Decompressor = Envoy::Compression::Decompressor;
namespace Compressor = Envoy::Compression::Compressor;
namespace RegexMatcher = Envoy::Extensions::Filters::Common::RegexMatcher;
namespace Algorithm = Envoy::Extensions::Filters::Common::Algorithm;
namespace algorithm = envoy::extensions::filters::http::common::algorithm;
using RegexMatcherPtr = std::shared_ptr<RegexMatcher::RegexMatcher>;

class RewriteRule {
public:
  RewriteRule(const v3::RewriteRule& rewrite);

public:
  const std::string& sensitiveType() { return sensitive_type_; }
  std::string rewrite(const std::string& data, RegexMatcher::EncodingType& encode_type);

private:
  const std::string sensitive_type_;
  std::unique_ptr<Algorithm::EncryptAlgorithmRewrite> encrypt_algorithmPtr_;
  std::unique_ptr<Algorithm::HashEncryptRewrite> hash_encryptPtr_;
  std::unique_ptr<Algorithm::CoverRewrite> coverPtr_;
  std::unique_ptr<Algorithm::ReplaceRewrite> replacePtr_;
  std::unique_ptr<Algorithm::ShuffleRewrite> shufflePtr_;
  std::unique_ptr<Algorithm::TransformRewrite> transformPtr_;
};
using RewriteRulePtr = std::shared_ptr<RewriteRule>;

class IdentifyRule {
public:
  IdentifyRule(const v3::IdentifyRule& identify);

public:
  const std::string& sensitveTypeName() { return sensitive_type_name_; }
  const std::string& fieldName() { return field_name_; }
  const std::string& dataContent() { return data_content_; }
  const v3::IdentifyRule_IdentifyType& identifyType() { return type_; }
  const v3::IdentifyRule_RecognitionLogic& recognitionLogic() { return logic_; }

private:
  const std::string sensitive_type_name_;
  v3::IdentifyRule_IdentifyType type_;
  v3::IdentifyRule_RecognitionLogic logic_;
  const std::string field_name_;
  const std::string data_content_;
};
using IdentifyRulePtr = std::shared_ptr<IdentifyRule>;

class Rule {
public:
  Rule(const v3::Rule& rule);

public:
  bool match(const Network::Address::InstanceConstSharedPtr& address,
             const Http::RequestHeaderMap& headers, const std::string& username) const;

  std::uint32_t id() const { return id_; }
  bool enable() const { return enable_; }
  const std::vector<Filters::Common::StrongMatcher::Matcher>& matcher() const { return matchers_; }
  const RewriteRulePtr& rewriteRule() const { return rewrite_rule_; }
  const IdentifyRulePtr& identifyRule() const { return identify_rule_; }
  const std::string& rewriteStrategyName() const { return rewrite_strategy_name_; }

private:
  const std::vector<Filters::Common::StrongMatcher::Matcher> matchers_;
  const std::uint32_t id_;
  const bool enable_;
  const RewriteRulePtr rewrite_rule_;
  const IdentifyRulePtr identify_rule_;
  const std::string rewrite_strategy_name_;
};
using RulePtr = std::unique_ptr<Rule>;

class IpWhitelist {
public:
  IpWhitelist(const v3::IpWhitelist& ipwhitelist);

public:
  bool matchIp(const Network::Address::InstanceConstSharedPtr& address);
  std::string sensitiveType() { return sensitive_type_; }
  bool enable() { return enable_; }
  bool sensitiveTypeAll() { return sensitive_type_all_; }

private:
  const bool enable_;
  // 敏感类型名称
  const std::string sensitive_type_;
  bool sensitive_type_all_{false};
  // ip白名单
  const std::vector<Filters::Common::IpWhitelist::IpWhitelistPtr> ip_whitelist_;
};

using IpWhitelistPtr = std::shared_ptr<IpWhitelist>;

class UserWhitelist {
public:
  UserWhitelist(const v3::UserWhitelist& userwhitelist);

public:
  bool matchUsername(const std::string& username);
  std::string sensitiveType() { return sensitive_type_; }
  bool enable() { return enable_; }
  bool sensitiveTypeAll() { return sensitive_type_all_; }

private:
  const bool enable_;
  // 敏感类型名称
  const std::string sensitive_type_;
  bool sensitive_type_all_{false};
  // 用户白名单
  const std::vector<Filters::Common::StrongMatcher::UserMatcherptr> user_whitelist_;
};
using UserWhitelistPtr = std::shared_ptr<UserWhitelist>;

class Whitelist {
public:
  Whitelist(const v3::Whitelist& whitelist);

public:
  std::vector<IpWhitelistPtr> ipWhiteListPtr() { return ip_white_list_; }
  std::vector<UserWhitelistPtr> userWhiteListPtr() { return user_white_list_; }

private:
  const std::vector<IpWhitelistPtr> ip_white_list_;
  const std::vector<UserWhitelistPtr> user_white_list_;
};
using WhitelistPtr = std::shared_ptr<Whitelist>;

// 全局配置
class ResponseRewriteGlobalConfig {
public:
  // friend class ResponseRewriteTest_encodedata_Test;
  ResponseRewriteGlobalConfig(const v3::ResponseRewriteGlobal&,
                              Decompressor::DecompressorFactoryPtr decompressor_gzip_factory,
                              Decompressor::DecompressorFactoryPtr decompressor_brotli_factory,
                              Compressor::CompressorFactoryPtr compressor_gzip_factory,
                              Compressor::CompressorFactoryPtr compressor_brotli_factory)
      : decompressor_gzip_factory_(std::move(decompressor_gzip_factory)),
        decompressor_brotli_factory_(std::move(decompressor_brotli_factory)),
        compressor_gzip_factory_(std::move(compressor_gzip_factory)),
        compressor_brotli_factory_(std::move(compressor_brotli_factory)) {}

public:
  Decompressor::DecompressorPtr makeDecompressorGzip() {
    return decompressor_gzip_factory_->createDecompressor("");
  }
  Decompressor::DecompressorPtr makeDecompressorBrotli() {
    return decompressor_brotli_factory_->createDecompressor("");
  }

  Compressor::CompressorPtr makeCompressorGzip() {
    return compressor_gzip_factory_->createCompressor();
  }
  Compressor::CompressorPtr makeCompressorBrotli() {
    return compressor_brotli_factory_->createCompressor();
  }

private:
  const Decompressor::DecompressorFactoryPtr decompressor_gzip_factory_;
  const Decompressor::DecompressorFactoryPtr decompressor_brotli_factory_;

  const Compressor::CompressorFactoryPtr compressor_gzip_factory_;
  const Compressor::CompressorFactoryPtr compressor_brotli_factory_;
};

using ResponseRewriteGlobalConfigSharedPtr = std::shared_ptr<ResponseRewriteGlobalConfig>;
using RegexMatcherPtr = std::shared_ptr<RegexMatcher::RegexMatcher>;

// VH及路由配置
class ResponseRewriteRouteConfig : public Router::RouteSpecificFilterConfig,
                                   public Logger::Loggable<Logger::Id::filter> {

public:
  ResponseRewriteRouteConfig(const v3::ResponseRewritePerRoute&);
  bool matchStatusCode(uint32_t status_code) const;

  // PB属性
public:
  inline bool enable() const { return enable_; }
  inline bool compressor_enable() const { return compressor_enable_; }
  inline const std::unordered_set<uint32_t>& status_codes() const { return status_codes_; }
  inline const std::vector<RulePtr>& rules() const { return rules_; }
  inline const Whitelist& whiteList() const { return white_list_; }
  static bool compareRulesById(std::unique_ptr<Rule>& lhs, std::unique_ptr<Rule>& rhs);
  uint32_t buffer() const { return buffer_; }
  const std::string username() const { return username_; }
  std::string executeIdentifyRule(const v3::IdentifyRule_IdentifyType identify_type,
                                  const v3::IdentifyRule_RecognitionLogic recognition_logic,
                                  const std::string& data_content, const std::string& field_name);
  inline RegexMatcherPtr matcher() const { return matcher_; }

private:
  const bool enable_;
  // 状态码
  const std::unordered_set<uint32_t> status_codes_;
  // 缓冲区大小
  const uint32_t buffer_;
  // 是否压缩数据
  const bool compressor_enable_;
  // 白名单
  const Whitelist white_list_;
  // 规则
  const std::vector<RulePtr> rules_;

  std::string username_;

  RegexMatcherPtr matcher_;
};

class ResponseRewrite : public Http::PassThroughFilterEx,
                        public Logger::Loggable<Logger::Id::filter> {
  friend class ResponseRewriteTest_encodeheader_Test;
  friend class ResponseRewriteTest_encodedata_Test;
  friend class ResponseRewriteTest_encodedata_xml_Test;

public:
  ResponseRewrite(ResponseRewriteGlobalConfigSharedPtr config,
                  const Server::Configuration::ServerFactoryContext& context)
      : Http::PassThroughFilterEx(context), filter_hcm_config_(config) {}

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance&, bool end_stream) override;
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool) override;
  Http::FilterDataStatus encodeData(Buffer::Instance&, bool end_stream) override;

  void matchCollectRules(const ResponseRewriteRouteConfig* filter_vh_config,
                         const ResponseRewriteRouteConfig* filter_route_config,
                         const Network::Address::InstanceConstSharedPtr downstreamAddress,
                         const Http::RequestHeaderMap& headers, const std::string& username);


  void onStreamComplete() override;

  v3::ResponseRewriteLog& getLog() { return log_; }

private:
  inline const ResponseRewriteRouteConfig* getConfig() const;

  bool isNeedRewrite(Http::ResponseHeaderMap& headers);

  void doRewrite(std::vector<RegexMatcher::MatchResult>& records,
                 const std::string_view& data_buf_in, Envoy::Buffer::OwnedImpl& data_buf_out);
  int threadIndex();

private:
  // 全局配置
  ResponseRewriteGlobalConfigSharedPtr filter_hcm_config_{};
  // 路由配置
  const ResponseRewriteRouteConfig* filter_config_{};

  Http::ResponseHeaderMap* headers_{};
  std::set<uint32_t> id_;

  Envoy::Compression::Decompressor::DecompressorPtr decompressor_{};
  Envoy::Compression::Compressor::CompressorPtr comperssor_{};
  RegexMatcher::EncodingType encode_type_;
  int thread_index_{-1};

  Http::RequestHeaderMap* saved_headers_ = nullptr;

  // 可视化日志
  v3::ResponseRewriteLog log_;
  static const std::string filter_name_;

  bool is_encode_buffer_full_{false};

private:
  inline const Envoy::Router::VirtualHostImpl* getVirtualHost() const;
  inline const ResponseRewriteRouteConfig*
  getVirtualHostConfig(const Envoy::Router::VirtualHostImpl* vh) const;
  inline const ResponseRewriteRouteConfig*
  getRouteConfig(const ResponseRewriteRouteConfig* filter_vh_config) const;
  inline const Network::Address::InstanceConstSharedPtr getDownstreamAddress() const;
  inline const std::string& getUpstreamName() const;
  inline bool getUserInfo(std::string& user_name) const;
};

} // namespace ResponseRewrite
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
