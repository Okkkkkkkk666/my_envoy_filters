#pragma once

#include "regex_matcher.h"
#include "regex_replacer.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {

class Regex {
public:
  Regex() = default;
  Regex(const Regex&) = delete;
  virtual ~Regex() = default;

public:
  /**
   * 创建一个多模正则匹配实例指针
   * @param mode
   *    正则匹配模式类型
   * @param care_position
   *    true 关心匹配位置
   *    false 不关心匹配位置
   *    注：当插件仅需要查找，而不需要提取或替换时，可以设置case_position为false，相比true的情况，性能可以提高10%左右
   * @param db_path
   *    指定正则引擎编译中间文件保存路径。
   *    db_path为空时: 将会重新编译中间文件，且中间文件保存在内存中，不写入文件系统
   *    db_path不为空时:
   * 如果db_path指向文件不存在，或者存在但与本次要编译的正则表达式不匹配，将会将重新编译中间文件，并保存在db_path指定的路径；
   *                    如果db_path指向文件已存在且正确，则直接从文件加载预编译中间文件到内存，不会重新编译中间文件。
   *
   *    注：
   * 当正则表达式比较多时，重新编译正则表达式会耗费数秒甚至几分钟的时间，因此如果正则表达式很少改变时，可指定db_path加速编译时间。
   * @return RegexMatcherSharedPtr
   */
  virtual RegexMatcherSharedPtr createRegexMatcher(RegexMode mode, bool care_position = false,
                                                   const std::string& db_path = std::string()) = 0;
  /**
   * 创建一个正则替换实例指针
   * @param ignore_case
   *    true 忽略大小写
   *    false 匹配大小写
   * @param match_expr
   *    匹配正则表达式
   * @param replace_expr
   *    替换正则表达式
   * @param type
   *    匹配正则表达式编码类型
   * @return RegexReplacerSharedPtr
   */
  virtual RegexReplacerSharedPtr createRegexReplacer(bool ignore_case,
                                                     const std::string& match_expr,
                                                     const std::string& replace_expr,
                                                     EncodingType type) = 0;
};
using RegexSharedPtr = std::shared_ptr<Regex>;

} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework