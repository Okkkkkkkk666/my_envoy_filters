#pragma once

#include <memory>
#include <string>
#include <vector>

#include "envoy/srhino_plugin_framework/v1_2_x/utility/http_parser.hpp"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace Regex {

using EncodingType = Utility::EncodingType;

/**
 * 用于正则替换，基于re2，支持utf8，gbk, iso编码
 */
class RegexReplacer {
public:
  RegexReplacer(const RegexReplacer&) = delete;
  RegexReplacer() = default;
  virtual ~RegexReplacer() = default;

public:
  /**
   * 执行正则替换
   * @param in
   *    目标字符串
   * @param code
   *    in 的数据编码类型
   * @param return
   *    true 替换成功
   *    false 失败
   */
  virtual bool replace(std::string& in, EncodingType code) = 0;
  /**
   * 获取器
   * @param return
   *   返回 ignore_case
   */
  virtual bool ignore_case() const = 0;
  /**
   * 获取器
   * @param return
   *   返回 match_expr
   */
  virtual const std::string& orig_match_expr() const = 0;
  /**
   * 获取器
   * @param return
   *   返回 replace_expr
   */
  virtual const std::string& replace_expr() const = 0;
};
using RegexReplacerSharedPtr = std::shared_ptr<RegexReplacer>;

} // namespace Regex
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework