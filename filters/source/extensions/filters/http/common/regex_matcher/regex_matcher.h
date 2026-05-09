#pragma once
#include <string>
#include <vector>
#include <memory>
#include <shared_mutex>

#include "source/common/common/logger.h"
#include "engine.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace RegexMatcher {

enum class RegexMode {
  RegexModeBlock = HS_MODE_BLOCK,
  RegexModeStream = HS_MODE_STREAM,
  RegexModeVector = HS_MODE_VECTORED,
};

class RegexMatcher : public Logger::Loggable<Logger::Id::filter> {
public:
  RegexMatcher(const RegexMatcher&) = delete;
  /**
   * @param type
   *    手动选择正则引擎， 仅用于测试时使用。正式项目中，应设置为RegexAuto，由程序自动选择引擎
   * @param care_position
   *    valid only in hyperscan模式。当插件仅需要查找，而不需要提取或替换时，可以设置case_position为false，相比true的情况，性能可以提高10%左右
   *    true:  match函数的result回传参数中会设置MatchResult对象的start_，end_
   *    false:  match函数的result回传参数中不会设置MatchResult对象的start_，end_。
   *            此时hyperscan和re2的回参一样，只设置了匹配到的模式id。
   * @param stream_mode
   *    valid only in hyperscan模式
   *    true:  流模式
   *    false: 块模式
  */
  [[deprecated]] RegexMatcher(RegexType type = RegexType::RegexAuto, bool care_position = false,
                              bool stream_mode = false)
      : RegexMatcher(stream_mode ? RegexMode::RegexModeStream : RegexMode::RegexModeBlock, type,
                     care_position) {}
  RegexMatcher(RegexMode mode, RegexType type = RegexType::RegexHyperscan, bool care_position = false);
  /**
   * @param db_path
   *   编译后的数据库保存路径。如果db文件已存在，则后面调用init函数时优先从db文件加载预编译数据库；如果db文件不存在，则init函数正常执行编译，编译成功后，将数据库写入db文件。
   * @param mode, type, care_position 略
  */
  RegexMatcher(const std::string& db_path, RegexMode mode,
               RegexType type = RegexType::RegexHyperscan, bool care_position = false);
  ~RegexMatcher() {};

  /**
   * 执行正则引擎编译构建初始化
   * 
   * @param expressions
   *    模式集
   * @param failed_exprs
   *    编译失败的模式
   * @param return
   *    0 成功
   *    -1 失败
  */
  int init(const std::vector<ExpressionView>& expressions, std::vector<ExpressionView>& failed_exprs);
  /**
   * 添加模式，如果要添加的模式已存在（根据id），则不允许添加
   * 注意： 添加模式会导致正则引擎重新构建
   * @param expression
   *      要添加的模式
   * @param return
   *      0  添加成功
   *      -1 添加失败
   */
  int addExpression(const ExpressionView& expression);
  /**
   * 删除模式
   * 注意： 删除模式会导致正则引擎重新构建
   * @param expr_id
   *    要删除的模式id
   * @param return
   *    0 删除成功
   *    -1 删除失败
   */
  int delExpression(const uint32_t expr_id);
  /**
   * 执行多模正则匹配
   * 注意： 如果使用的hyperscan引擎，多线程调用时一定要设置thread_index（线程索引的获取方法参考 regex_matcher_demon 插件)。否则会出现内存异常！
   * @param data
   *    要匹配的数据
   * @param len
   *    要匹配的数据长度
   * @param results
   *    匹配结果 
   * @param encode
   *    匹配数据的编码格式
   * @param thread_index
   *    当前线程索引
   * @param return
   *    0 匹配正常，是否匹配到要检查results的内容
   *    -1 匹配过程出错
  */
  int match(const char* data, size_t len, std::vector<MatchResult>& results, EncodingType encode = EncodingType::UTF8, int thread_index = 0);
  int match(const std::string& data, std::vector<MatchResult>& results, EncodingType encode = EncodingType::UTF8, int thread_index = 0);
  int match(const std::string_view& data, std::vector<MatchResult>& results, EncodingType encode = EncodingType::UTF8, int thread_index = 0);
  /**
   * 创建流
   * @param ctx
   *    流上下文
   * @param stream_id
   *    用户自定义流ID。可选
   * @param encode
   *    匹配数据的编码格式
   * @param return
   *    0 创建成功
   *    -1 创建失败
  */
  int streamOpen(StreamContext& ctx, uint32_t stream_id = 0, EncodingType encode = EncodingType::UTF8);
  /**
   * 流扫描
   * @param ctx
   *    流上下文
   * @param data
   *    要扫描的数据
   * @param results
   *    匹配结果 
   * @param return
   *    0 匹配正常，是否匹配到要检查results的内容
   *    -1 匹配过程出错
  */
  int streamScan(StreamContext& ctx, const std::string_view& data, std::vector<MatchResult>& results, int thread_index = 0);
  /**
   * 关闭流
   * 注意： 关闭流时，虽然没有提供数据，但也可能会产生扫描结果，使用时要注意这一点儿
   * @param ctx
   *    流上下文
   * @param results
   *    匹配结果。
  */
  int streamClose(StreamContext& ctx, std::vector<MatchResult>& results, int thread_index = 0);
  /**
   * vector模式的多模正则匹配
   * @param data
   *    要扫描的数据组成的数组
   * @param length
   *    前面data中各元素的长度
   * @param results
   *    匹配结果
   * @param return
   *    0 匹配正常，是否匹配到要检查results的内容
   *    -1 匹配过程出错
  */
  int vectorMatch(const std::vector<const char*>& data, const std::vector<uint32_t>& length, std::vector<MatchResult>& results, EncodingType encode, int thread_index = 0);

public:
  inline RegexType type() const {return type_;}
  inline bool care_position() const {return care_position_;}
  [[deprecated]] inline bool stream_mode() const {return (mode_ == RegexMode::RegexModeStream);}
  inline RegexMode mode() const {return mode_;}
private:
  /**
   * 构造正则引擎实例
   * @param type
   *      选择的算法类型
   * @param expressions
   *      模式集
   * @param failed_exprs
   *    编译失败的模式
   * @param engine
   *      保存创建的引擎实例指针
   * @param encode
   *      引擎编码
   * @return
   *      0 成功
   *      -1 失败
   */
  int createRegexEncodingEngine(RegexType type, const std::vector<ExpressionViewPtr>& expressions,
                                std::vector<ExpressionView>& failed_exprs, EnginePtr &engine, EncodingType encode);
  /**
   * 创建正则引擎集合
   * @param failed_exprs
   *    编译失败的模式
   * @param engines
   *    用于保存创建的正则引擎
  */
  int createRegexEngine(std::vector<ExpressionView>& failed_exprs, std::vector<EnginePtr> &engines);
private:
  std::shared_mutex mtx_;
  std::array<std::string, EncodingType::MAX> db_path_;
  RegexType type_;
  bool care_position_{false};
  RegexMode mode_{RegexMode::RegexModeBlock};
  std::vector<EnginePtr> engines_;
  std::vector<ExpressionViewPtr> expr_list_;
  std::map<uint32_t, ExpressionViewPtr> expr_map_;
};

class RegexUtilities {
public:
  static EncodingType getEncodeType(const std::string_view& content_type);
  static bool convertImpl(const std::string_view& pattern, std::string& output,
                  const std::string_view& from_encode,
                  const std::string_view& to_encode);
  static std::shared_ptr<re2::RE2> EncodingRe(bool ignore_case, const std::string& utf8_pattern, EncodingType type);
};
using RegexMatcherPtr = std::shared_ptr<RegexMatcher>;

/**
 * 用于正则替换，基于re2，支持utf8，gbk, iso编码
*/
class RegexReplacer : public Logger::Loggable<Logger::Id::filter> {
public:
  RegexReplacer(bool ignore_case, const std::string& match_expr, const std::string& replace_expr);
  virtual ~RegexReplacer() {}
  virtual bool replace(std::string& in, EncodingType code);

  bool ignore_case() const { return ignore_case_; }
  const std::string& match_expr() const { return match_expr_; }
  const std::string& replace_expr() const { return replace_expr_; }

private:
  bool ignore_case_;
  std::string match_expr_;
  std::string replace_expr_;
  std::vector<std::shared_ptr<re2::RE2>> match_re2s_;
  std::vector<std::string> replace_exprs_;
};


} // namespace RegexMatcher
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy