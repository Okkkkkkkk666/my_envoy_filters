#pragma once

#include <shared_mutex>
#include "source/common/common/logger.h"
#include "envoy/srhino_plugin_framework/v1_4_x/libs/regex/regex_matcher.h"
#include "hyperscan_engine.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {

class RegexMatcherImpl : public RegexMatcher,
                         public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  RegexMatcherImpl(const RegexMatcherImpl&) = delete;
  RegexMatcherImpl(RegexMode mode, bool care_position = false,
                   const std::string& db_path = std::string());
  ~RegexMatcherImpl() = default;

public:
  bool init(const std::vector<ExpressionView>& expressions,
            std::vector<ExpressionView>& failed_exprs) override;
  bool init(const std::vector<RuleConfig>& expressions,
            std::vector<RuleConfig>& failed_exprs) override;

  bool initWithDataTagRules(const std::vector<RuleConfig>& expressions,
                            std::vector<RuleConfig>& failed_exprs);

  bool addExpression(const ExpressionView& expression) override;
  bool delExpression(const uint32_t expr_id) override;

  bool match(const char* data, size_t len, std::vector<MatchResult>& results,
             EncodingType encode = EncodingType::UTF8) override;
  bool match(const std::string& data, std::vector<MatchResult>& results,
             EncodingType encode = EncodingType::UTF8) override;
  bool match(const std::string_view& data, std::vector<MatchResult>& results,
             EncodingType encode = EncodingType::UTF8) override;
  bool pcreMatch(const char* data, size_t len, std::vector<MatchResult>& results,
                 EncodingType encode = EncodingType::UTF8) override;
  bool pcreMatch(const std::string& data, std::vector<MatchResult>& results,
                 EncodingType encode = EncodingType::UTF8) override;
  bool pcreMatch(const std::string_view& data, std::vector<MatchResult>& results,
                 EncodingType encode = EncodingType::UTF8) override;

  bool dataTagPcreMatch(const std::string_view& data, std::vector<MatchResult>& results,
                        EncodingType encode = EncodingType::UTF8);

  bool pcreBlockMatch(const char* data, size_t len, std::vector<MatchResult>& results,
                      EncodingType encode = EncodingType::UTF8) override;
  bool pcreBlockMatch(const std::string& data, std::vector<MatchResult>& results,
                      EncodingType encode = EncodingType::UTF8) override;
  bool pcreBlockMatch(const std::string_view& data, std::vector<MatchResult>& results,
                      EncodingType encode = EncodingType::UTF8) override;

  bool dataTagPcreBlockMatch(const char* data, size_t len, std::vector<MatchResult>& results,
                             bool left_most) override;
  bool dataTagPcreBlockMatch(const std::string& data, std::vector<MatchResult>& results,
                             bool left_most) override;
  bool dataTagPcreBlockMatch(const std::string_view& data, std::vector<MatchResult>& results,
                             bool left_most) override;

  bool streamOpen(StreamContext& ctx, uint32_t stream_id = 0,
                  EncodingType encode = EncodingType::UTF8) override;
  bool streamScan(StreamContext& ctx, const std::string_view& data,
                  std::vector<MatchResult>& results) override;
  bool streamClose(StreamContext& ctx, std::vector<MatchResult>& results) override;
  bool vectorMatch(const std::vector<const char*>& data, const std::vector<uint32_t>& length,
                   std::vector<MatchResult>& results,
                   EncodingType encode = EncodingType::UTF8) override;
  RegexMode mode() const override { return mode_; }
  bool care_position() const override { return care_position_; }
  const std::string& db_path() const override { return db_path_; }
  bool isEngineReady(EncodingType encode = EncodingType::UTF8) const override {
    return engines_[encode] ? true : false;
  }

private:
  /**
   * 构造不同编码类型的正则引擎实例
   * @param encode
   *    引擎编码
   * @param failed_exprs
   *    编译失败的模式
   * @return
   *    保存创建的引擎实例指针
   */
  HyperscanEngineSharedPtr createRegexEngine(EncodingType encode,
                                             std::vector<ExpressionView>& failed_exprs);

  /**
   * 构造DataTag引擎实例
   * @param failed_exprs
   *    编译失败的模式
   * @return
   *    保存创建的DataTag引擎实例指针
   */
  HyperscanEngineSharedPtr createDataTagEngine(std::vector<RuleConfig>& failed_exprs);

  /**
   * 构造正则引擎实例
   * @param encode
   *    引擎编码
   * @param expressions
   *    模式集
   * @param failed_exprs
   *    编译失败的模式
   * @return
   *    保存创建的引擎实例指针
   */
  HyperscanEngineSharedPtr
  createRegexEncodingEngine(EncodingType encode,
                            const std::vector<ExpressionViewSharedPtr>& expressions,
                            std::vector<ExpressionView>& failed_exprs);

  /**
   * 构造DataTag引擎实例
   * @param encode
   *    引擎编码
   * @param expressions
   *    模式集
   * @param failed_exprs
   *    编译失败的模式
   * @return
   *    保存创建的DataTag引擎实例指针
   */
  HyperscanEngineSharedPtr createDataTagEncodingEngine(EncodingType encode,
                                                       const std::vector<RuleConfig>& expressions,
                                                       std::vector<RuleConfig>& failed_exprs);

private:
  const RegexMode mode_;
  const bool care_position_;
  const std::string db_path_;

  std::shared_mutex mtx_;
  std::array<std::string, EncodingType::MAX> db_paths_;
  std::array<HyperscanEngineSharedPtr, EncodingType::MAX> engines_;
  HyperscanEngineSharedPtr data_tag_engines_;
  std::vector<ExpressionViewSharedPtr> expr_list_;
  std::map<uint32_t, ExpressionViewSharedPtr> expr_map_;

  std::vector<RuleConfig> data_tag_rule_list_;
  std::map<uint32_t, RuleConfig> data_tag_rule_map_;
};

} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework