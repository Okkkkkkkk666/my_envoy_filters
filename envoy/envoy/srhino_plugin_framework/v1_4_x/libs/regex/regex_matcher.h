#pragma once

#include <memory>
#include <string>
#include <vector>

#include "envoy/srhino_plugin_framework/v1_4_x/utility/http_parser.hpp"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {

using EncodingType = Utility::EncodingType;

enum class RegexMode {
  RegexModeBlock,
  RegexModeStream,
  RegexModeVector,
};

class ExpressionView;
using ExpressionViewSharedPtr = std::shared_ptr<ExpressionView>;
class ExpressionView {
public:
  ExpressionView() {}
  ExpressionView(const std::string& expr, uint32_t id, bool ignore_case = false,
                 bool utf_8_flag = true)
      : expr_(expr), id_(id), ignore_case_(ignore_case), utf_8_flag_(utf_8_flag) {}
  ExpressionView(const ExpressionViewSharedPtr ev) {
    expr_ = ev->expr();
    id_ = ev->id();
    ignore_case_ = ev->ignore_case();
    utf_8_flag_ = ev->utf_8_flag();
  }
  inline const std::string& expr() const { return expr_; }
  inline uint32_t id() const { return id_; }
  inline bool ignore_case() const { return ignore_case_; }
  inline bool utf_8_flag() const { return utf_8_flag_; }

private:
  std::string expr_{};
  uint32_t id_{};
  bool ignore_case_{false}; /* 不区分大小写 */
  bool utf_8_flag_{true};   /* 设置utf-8 */
};

// 约束
struct ConstraintRule {
  // 正则表达式
  std::string expr;
  // 是否取反
  bool invert{false};
  // 是否匹配空串（如果开头或结尾没有数据，是否匹配）
  bool match_null{false};
  // 最大扫描长度（开头）
  uint32_t max_match_length{5};
};

// 单条数据标签规则
struct RuleConfig {
  // 规则id 注意，同一模式集内的ID必须唯一
  uint64_t id{0};
  // 规则名称，保留
  std::string name;
  // 规则描述，保留
  std::string description;
  // 数据标签规则 & 正则表达式
  std::string expr;
  // 是否忽略大小写
  bool case_less{false};
  // 是否开启left_most多模扫描，默认false不开启
  // 开启条件：主规则匹配可能超过128字节&&(有before约束 || 有contain约束 ||需要定位from)
  bool left_most{false};
  // 包含约束
  std::vector<ConstraintRule> contains;
  // 开头约束
  ConstraintRule before;
  // 末尾约束
  ConstraintRule after;
};
using RuleConfigSharedPtr = std::shared_ptr<RuleConfig>;

class MatchResult {
public:
  MatchResult() {}
  MatchResult(uint32_t id, uint32_t start = 0, uint32_t end = 0)
      : id_(id), start_(start), end_(end) {}
  inline uint32_t id() const { return id_; }
  inline uint32_t start() const { return start_; }
  inline uint32_t end() const { return end_; }
  inline bool flag() const { return flag_; }
  inline void set_id(uint32_t id) { id_ = id; }
  inline void set_start(uint32_t start) { start_ = start; }
  inline void set_end(uint32_t end) { end_ = end; }
  inline void set_flag(bool flag) { flag_ = flag; }
  static bool cmp(const MatchResult& r1, const MatchResult& r2) { return r1.start() < r2.start(); }

private:
  /**
   *  expression id
   */
  uint32_t id_{};
  /**
   *  start position.
   */
  uint32_t start_{};
  /**
   *  end position.
   */
  uint32_t end_{};
  /**
   * 是否有效
   */
  bool flag_{true};
};

class StreamContext {
public:
  void* id = nullptr;     // 流模式下标识一个流的唯一ID，不要修改
  uint32_t stream_id = 0; // 用户侧标识一个流的ID
  EncodingType encode = EncodingType::UTF8;
};

class RegexMatcher {
public:
  RegexMatcher(const RegexMatcher&) = delete;
  RegexMatcher() = default;
  virtual ~RegexMatcher() = default;

  /**
   * 执行正则引擎编译构建初始化
   *
   * @param expressions
   *    模式集
   * @param failed_exprs
   *    编译失败的模式
   * @param return
   *    true 成功
   *    false 失败
   */
  virtual bool init(const std::vector<ExpressionView>& expressions,
                    std::vector<ExpressionView>& failed_exprs) = 0;
  virtual bool init(const std::vector<RuleConfig>& expressions,
                    std::vector<RuleConfig>& failed_exprs) = 0;
  /**
   * 添加模式，如果要添加的模式已存在（根据id），则不允许添加
   * 注意： 添加模式会导致正则引擎重新构建
   * @param expression
   *      要添加的模式
   * @param return
   *      true  添加成功
   *      false 添加失败
   */
  virtual bool addExpression(const ExpressionView& expression) = 0;
  /**
   * 删除模式
   * 注意： 删除模式会导致正则引擎重新构建
   * @param expr_id
   *    要删除的模式id
   * @param return
   *    true 删除成功
   *    false 删除失败
   */
  virtual bool delExpression(const uint32_t expr_id) = 0;
  /**
   * 执行多模正则匹配
   * @param data
   *    要匹配的数据
   * @param len
   *    要匹配的数据长度
   * @param results
   *    匹配结果
   * @param encode
   *    匹配数据的编码格式
   * @param return
   *    true 匹配正常，是否匹配到要检查results的内容
   *    false 匹配过程出错
   */
  virtual bool match(const char* data, size_t len, std::vector<MatchResult>& results,
                     EncodingType encode = EncodingType::UTF8) = 0;
  virtual bool match(const std::string& data, std::vector<MatchResult>& results,
                     EncodingType encode = EncodingType::UTF8) = 0;
  virtual bool match(const std::string_view& data, std::vector<MatchResult>& results,
                     EncodingType encode = EncodingType::UTF8) = 0;
  /**
   * 与match函数不同的是pcreMatch支持正则表达式含有PCRE语法。
   */
  virtual bool pcreMatch(const char* data, size_t len, std::vector<MatchResult>& results,
                         EncodingType encode = EncodingType::UTF8) = 0;
  virtual bool pcreMatch(const std::string& data, std::vector<MatchResult>& results,
                         EncodingType encode = EncodingType::UTF8) = 0;
  virtual bool pcreMatch(const std::string_view& data, std::vector<MatchResult>& results,
                         EncodingType encode = EncodingType::UTF8) = 0;

  /**
   * 与pcreMatch函数不同的是pcreBlockMatch在原基础上使用块扫描，先使用hyperscan匹配一遍再用pcre去扫可以提高pcre的扫描性能。
   */
  virtual bool pcreBlockMatch(const char* data, size_t len, std::vector<MatchResult>& results,
                              EncodingType encode = EncodingType::UTF8) = 0;
  virtual bool pcreBlockMatch(const std::string& data, std::vector<MatchResult>& results,
                              EncodingType encode = EncodingType::UTF8) = 0;
  virtual bool pcreBlockMatch(const std::string_view& data, std::vector<MatchResult>& results,
                              EncodingType encode = EncodingType::UTF8) = 0;

  virtual bool dataTagPcreBlockMatch(const char* data, size_t len,
                                     std::vector<MatchResult>& results, bool left_most) = 0;
  virtual bool dataTagPcreBlockMatch(const std::string& data, std::vector<MatchResult>& results,
                                     bool left_most) = 0;
  virtual bool dataTagPcreBlockMatch(const std::string_view& data,
                                     std::vector<MatchResult>& results, bool left_most) = 0;
  /**
   * 创建流
   * @param ctx
   *    流上下文
   * @param stream_id
   *    用户自定义流ID。可选
   * @param encode
   *    匹配数据的编码格式
   * @param return
   *    true 创建成功
   *    false 创建失败
   */
  virtual bool streamOpen(StreamContext& ctx, uint32_t stream_id = 0,
                          EncodingType encode = EncodingType::UTF8) = 0;
  /**
   * 流扫描
   * @param ctx
   *    流上下文
   * @param data
   *    要扫描的数据
   * @param results
   *    匹配结果
   * @param return
   *    true 匹配正常，是否匹配到要检查results的内容
   *    false 匹配过程出错
   */
  virtual bool streamScan(StreamContext& ctx, const std::string_view& data,
                          std::vector<MatchResult>& results) = 0;
  /**
   * 关闭流
   * 注意： 关闭流时，虽然没有提供数据，但也可能会产生扫描结果，使用时要注意这一点儿
   * @param ctx
   *    流上下文
   * @param results
   *    匹配结果。
   */
  virtual bool streamClose(StreamContext& ctx, std::vector<MatchResult>& results) = 0;
  /**
   * vector模式的多模正则匹配
   * @param data
   *    要扫描的数据组成的数组
   * @param length
   *    前面data中各元素的长度
   * @param results
   *    匹配结果
   * @param return
   *    true 匹配正常，是否匹配到要检查results的内容
   *    false 匹配过程出错
   */
  virtual bool vectorMatch(const std::vector<const char*>& data,
                           const std::vector<uint32_t>& length, std::vector<MatchResult>& results,
                           EncodingType encode = EncodingType::UTF8) = 0;
  /**
   * 获取器
   * @param return
   *    返回 mode
   */
  virtual RegexMode mode() const = 0;
  /**
   * 获取器
   * @param return
   *    返回 care_position
   */
  virtual bool care_position() const = 0;
  /**
   * 获取器
   * @param return
   *    返回 db_path
   */
  virtual const std::string& db_path() const = 0;
  /**
   * 相应编码类型的 engine 是否初始化成功
   */
  virtual bool isEngineReady(EncodingType encode = EncodingType::UTF8) const = 0;
};
using RegexMatcherSharedPtr = std::shared_ptr<RegexMatcher>;

} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework