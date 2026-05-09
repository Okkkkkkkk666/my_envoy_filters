#pragma once
#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include "source/common/common/non_copyable.h"
#include "re2/re2.h"
#include "source/common/common/assert.h"
#include "envoy/buffer/buffer.h"
#include "filters/api/envoy/extensions/filters/http/response_rewrite/v3/response_rewrite.pb.h"
#include "filters/source/extensions/filters/http/common/encode_regex/encoding_converter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ResponseRewrite {

namespace v3 = envoy::extensions::filters::http::response_rewrite::v3;
namespace EncodingConverter = Envoy::Extensions::Filters::Common::EncodingConverter;

class RewriteRule {
public:
  RewriteRule(const v3::RewriteRule& rule);
  // 处理规则替换
  bool rewrite(std::string& data, const EncodingConverter::EncodingType code) const;

  // 获取修改表达式
  inline const std::string& replace_gbk() const { return replace_gbk_; }
  inline const std::string& replace_utf() const { return replace_utf_; }
  // 获取规则是否启动
  inline const bool& rule_enable() const { return rule_enable_; }
  inline const std::string& chinese_name() const { return chinese_name_; }
  // 获取正则匹配表达式
  inline const std::shared_ptr<re2::RE2> regexes(const EncodingConverter::EncodingType code) const {
    return regexes_->getEncodeRegex(code);
  }

private:
  // 修改表达式
  std::string replace_utf_;
  std::string replace_gbk_;
  // 启用规则
  bool rule_enable_;
  std::string chinese_name_;
  // 正则表达式
  EncodingConverter::EncodingRePtr regexes_;
};
using RewriteRuleSharedPtr = std::shared_ptr<const RewriteRule>;

} // namespace ResponseRewrite
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy