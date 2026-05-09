#include "regex_matcher_impl.h"
#include "regex_utility.hpp"
#ifdef __aarch64__
#include "third-party/hyperscan-aarch64/src/hs.h"
#else
#include "third-party/hyperscan/src/hs.h"
#endif

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Regex {

static const std::unordered_map<RegexMode, uint32_t> RegexModeMap = {
    {RegexMode::RegexModeBlock, HS_MODE_BLOCK},
    {RegexMode::RegexModeStream, HS_MODE_STREAM},
    {RegexMode::RegexModeVector, HS_MODE_VECTORED}};

RegexMatcherImpl::RegexMatcherImpl(RegexMode mode, bool care_position, const std::string& db_path)
    : mode_(mode), care_position_(care_position), db_path_(db_path) {
  for (auto i = 0; i < EncodingType::MAX; i++) {
    if (!db_path_.empty()) {
      db_paths_[i] = db_path_ + "_encoding" + std::to_string(i);
    } else {
      db_paths_[i] = std::string();
    }

    engines_[i] = nullptr;
  }
}

bool RegexMatcherImpl::init(const std::vector<ExpressionView>& expressions,
                            std::vector<ExpressionView>& failed_exprs) {
  if (HS_SUCCESS != hs_valid_platform()) {
    ENVOY_LOG(warn, "cpu do not support ssse3, use re2 instead");
    return false;
  }
  if (RegexModeMap.find(mode_) == RegexModeMap.end()) {
    return false;
  }

  if (expressions.size() == 0)
    return false;

  // 保存传入的模式集
  for (auto it : expressions) {
    if (it.expr().length() == 0) {
      failed_exprs.push_back(it);
      expr_list_.clear();
      expr_map_.clear();
      return false;
    }
    ExpressionViewSharedPtr pExpr = std::make_shared<ExpressionView>(it);
    expr_list_.push_back(pExpr);
    if (expr_map_.end() == expr_map_.find(it.id())) {
      expr_map_[it.id()] = pExpr;
    } else {
      ENVOY_LOG(warn, "Expression: {}, id: {} exist!", pExpr->expr(), pExpr->id());
      failed_exprs.push_back(it);
      expr_list_.clear();
      expr_map_.clear();
      return false;
    }
  }

  // 创建正则引擎
  std::array<HyperscanEngineSharedPtr, EncodingType::MAX> engines = {};
  bool success = false;
  for (int i = EncodingType::ISO_8859_1; i < EncodingType::MAX; i++) {
    engines[i] = createRegexEngine(static_cast<EncodingType>(i), failed_exprs);
    if (engines[i]) {
      success = true;
    } else {
      ENVOY_LOG(warn, "create regex engine for encoding: {} failed", i);
    }
  }
  engines_ = engines;
  return success;
}


/**
 * 对正则表达式转换成不同的字符集，并编译。
 *
 * 转换规则：
 * 1. 原正则表达式集默认是utf-8编码
 *
 * 2. 如果正则表达式转换成另一种字符集失败，那么这个正则表达式就不编译。
 *  举例： 比如正则表达式中含有utf-8字符 "中国", 将其转换成iso-8859-1是失败的，因为iso-8859-1是单字节编码。
 *  那么如果输入数据是iso-8859-1编码，那么一定匹配不上这条正则表达式，所以这个正则表达式不编译，不影响匹配结果。
 *
 * 3. 如果发生了转换，则禁止忽略大小写，否则会产生误匹配
 *  理论上，如果hyperscan不使用HS_FLAG_UTF8编码，当正则表达式中出现多字节字符时，就不能使用忽略大小写模式。
 *  举例： 正则表达式 '侲侳侴侶'的编码为 '82 45 82 46 82 47 82 48'
 *        文本的编码为 '82 65 82 66 82 67 82 68'，由于不忽略大小写，hyperscan是按字节一个一个匹配的，导致误匹配。
 *
 */
HyperscanEngineSharedPtr RegexMatcherImpl::createRegexEngine(EncodingType encode, std::vector<ExpressionView>& failed_exprs) {
  switch(encode) {
    case EncodingType::ISO_8859_1: {
      std::vector<ExpressionViewSharedPtr> new_exprs;
      for (const auto& it : expr_list_) {
        std::string val;
        if (RegexUtilities::convertImpl(it->expr(), val, "UTF-8", "ISO-8859-1")) {
          new_exprs.emplace_back(std::make_shared<ExpressionView>(val, it->id(), it->ignore_case(), false));
        } else {
          ENVOY_LOG(warn, "expression: {} convert to from utf-8 to iso-8859-1 failed", it->expr());
          // return false;
        }
      }
      return createRegexEncodingEngine(encode, new_exprs, failed_exprs);
      break;
    }
    case EncodingType::UTF8: {
      return createRegexEncodingEngine(encode, expr_list_, failed_exprs);
    }
    case EncodingType::GBK: {
      std::vector<ExpressionViewSharedPtr> new_exprs;
      for (const auto& it : expr_list_) {
        std::string val;
        if (RegexUtilities::convertImpl(it->expr(), val, "UTF-8", "GBK")) {
          bool ignore_case = it->ignore_case();
          if (val != it->expr()) { // 禁止忽略大小写，否则会产生误匹配
            if (ignore_case) {
              ENVOY_LOG(warn, "expr id: {}, ignore_case changed to case sensitive", it->id());
              ignore_case = false;
            }
          }
          new_exprs.emplace_back(std::make_shared<ExpressionView>(val, it->id(), ignore_case, false));
        } else {
          ENVOY_LOG(warn, "expression: {} convert to from utf-8 to gbk failed", it->expr());
          // return false;
        }
      }
      return createRegexEncodingEngine(encode, new_exprs, failed_exprs);
      break;
    }
    case EncodingType::GB2312: {
      std::vector<ExpressionViewSharedPtr> new_exprs;
      for (const auto& it : expr_list_) {
        std::string val;
        if (RegexUtilities::convertImpl(it->expr(), val, "UTF-8", "GB2312")) {
          bool ignore_case = it->ignore_case();
          if (val != it->expr()) {
            if (ignore_case) {
              ENVOY_LOG(warn, "expr id: {}, ignore_case changed to case sensitive", it->id());
              ignore_case = false;
            }
          }
          new_exprs.emplace_back(std::make_shared<ExpressionView>(val, it->id(), ignore_case, false));
        } else {
          ENVOY_LOG(warn, "expression: {} convert to from utf-8 to gb2312 failed", it->expr());
          // return false;
        }
      }
      return createRegexEncodingEngine(encode, new_exprs, failed_exprs);
      break;
    }
    default:
      break;
  }
  return nullptr;
}

HyperscanEngineSharedPtr
RegexMatcherImpl::createRegexEncodingEngine(EncodingType encode,
                                            const std::vector<ExpressionViewSharedPtr>& expressions,
                                            std::vector<ExpressionView>& failed_exprs) {
  HyperscanEngineSharedPtr engine =
      std::make_shared<HyperscanEngine>(db_paths_[encode], care_position_, RegexModeMap.at(mode_));
  if(!engine->init(expressions, failed_exprs)) {
    engine = nullptr;
  }
  return engine;
}

bool RegexMatcherImpl::addExpression(const ExpressionView& expression) {
  std::unique_lock<std::shared_mutex> lock(mtx_);
  if (expr_map_.end() != expr_map_.find(expression.id())) {
    return false;
  }
  if (expression.expr().length() == 0) {
    return false;
  }

  std::shared_ptr<ExpressionView> pExpr = std::make_shared<ExpressionView>(expression);
  expr_list_.push_back(pExpr);
  expr_map_[pExpr->id()] = pExpr;

  std::vector<ExpressionView> failed_exprs;
  std::array<HyperscanEngineSharedPtr, EncodingType::MAX> engines = {};
  for (int i = EncodingType::ISO_8859_1; i < EncodingType::MAX; i++) {
    engines[i] = createRegexEngine(static_cast<EncodingType>(i), failed_exprs);
  }
  engines_ = engines;
  return true;
}

bool RegexMatcherImpl::delExpression(const uint32_t expr_id) {
  std::unique_lock<std::shared_mutex> lock(mtx_);
  auto it = expr_map_.find(expr_id);
  if (it == expr_map_.end()) {
    return false;
  }
  auto pExpr = it->second;
  expr_map_.erase(expr_id);
  for (auto it = expr_list_.begin(); it != expr_list_.end(); it++) {
    if ((*it)->id() == expr_id) {
      // assert(*it == pExpr);
      expr_list_.erase(it);
      break;
    }
  }

  std::vector<ExpressionView> failed_exprs;
  std::array<HyperscanEngineSharedPtr, EncodingType::MAX> engines = {};
  for (int i = EncodingType::ISO_8859_1; i < EncodingType::MAX; i++) {
    engines[i] = createRegexEngine(static_cast<EncodingType>(i), failed_exprs);
  }
  engines_ = engines;
  return true;
}

bool RegexMatcherImpl::match(const char* data, size_t len, std::vector<MatchResult>& results,
                             EncodingType encode) {
  return match(std::string_view(data, len), results, encode);
}

bool RegexMatcherImpl::match(const std::string& data, std::vector<MatchResult>& results,
                             EncodingType encode) {
  return match(data.c_str(), data.length(), results, encode);
}

bool RegexMatcherImpl::match(const std::string_view& data, std::vector<MatchResult>& results,
                             EncodingType encode) {
  if (data.length() == 0) {
    return true;
  }

  if (mode_ != RegexMode::RegexModeBlock) {
    return false;
  }

  std::shared_lock<std::shared_mutex> lock(mtx_);

  if (!engines_[encode]) {
    ENVOY_LOG(error, "engines_ is null for encode type: {}", encode);
    return false;
  }
  return engines_[encode]->match(data, results);
}

bool RegexMatcherImpl::pcreMatch(const char* data, size_t len, std::vector<MatchResult>& results,
                                 EncodingType encode) {
  return pcreMatch(std::string_view(data, len), results, encode);
}

bool RegexMatcherImpl::pcreMatch(const std::string& data, std::vector<MatchResult>& results,
                                 EncodingType encode) {
  return pcreMatch(data.c_str(), data.length(), results, encode);
}

bool RegexMatcherImpl::pcreMatch(const std::string_view& data, std::vector<MatchResult>& results,
                                 EncodingType encode) {
  if (data.length() == 0) {
    return true;
  }

  if (mode_ != RegexMode::RegexModeBlock) {
    return false;
  }

  std::shared_lock<std::shared_mutex> lock(mtx_);

  if (!engines_[encode]) {
    ENVOY_LOG(error, "engines_ is null for encode type!");
    return false;
  }
  return engines_[encode]->pcreMatch(data, results);
}

bool RegexMatcherImpl::streamOpen(StreamContext& ctx, uint32_t stream_id, EncodingType encode) {
  if (mode_ != RegexMode::RegexModeStream) {
    return false;
  }

  std::shared_lock<std::shared_mutex> lock(mtx_);
  ctx.encode = encode;

  if (!engines_[encode]) {
    ENVOY_LOG(error, "engines_ is null for encode type!");
    return false;
  }
  return engines_[encode]->streamOpen(ctx, stream_id);
}

bool RegexMatcherImpl::streamScan(StreamContext& ctx, const std::string_view& data,
                                  std::vector<MatchResult>& results) {
  if (mode_ != RegexMode::RegexModeStream) {
    return false;
  }

  std::shared_lock<std::shared_mutex> lock(mtx_);

  if (!engines_[ctx.encode]) {
    ENVOY_LOG(error, "engines_ is null for ctx.encode type!");
    return false;
  }
  return engines_[ctx.encode]->streamScan(ctx, data, results);
}

bool RegexMatcherImpl::streamClose(StreamContext& ctx, std::vector<MatchResult>& results) {
  if (mode_ != RegexMode::RegexModeStream) {
    return false;
  }

  std::shared_lock<std::shared_mutex> lock(mtx_);

  if (!engines_[ctx.encode]) {
    ENVOY_LOG(error, "engines_ is null for ctx.encode type!");
    return false;
  }
  return engines_[ctx.encode]->streamClose(ctx, results);
}

bool RegexMatcherImpl::vectorMatch(const std::vector<const char*>& data,
                                   const std::vector<uint32_t>& length,
                                   std::vector<MatchResult>& results, EncodingType encode) {
  if (mode_ != RegexMode::RegexModeVector) {
    return false;
  }
  if (data.size() != length.size()) {
    return false;
  }
  if (data.size() == 0) {
    return true;
  }

  std::shared_lock<std::shared_mutex> lock(mtx_);

  if (!engines_[encode]) {
    ENVOY_LOG(error, "engines_ is null for encode type!");
    return false;
  }
  return engines_[encode]->match(data, length, results);
}

} // namespace Regex
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework
