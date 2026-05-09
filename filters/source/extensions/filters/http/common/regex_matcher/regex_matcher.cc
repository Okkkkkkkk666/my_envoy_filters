#include <iconv.h>
#include "regex_matcher.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace RegexMatcher {

RegexMatcher::RegexMatcher(RegexMode mode, RegexType type, bool care_position)
    : care_position_(care_position), mode_(mode) {
  //根据CPU选择算法
  bool ssse3_support = (HS_SUCCESS == hs_valid_platform());
  type_ = RegexType::RegexHyperscan;

  switch (type) {
  case RegexType::RegexAuto:
    if (!ssse3_support) {
      type_ = RegexType::RegexRe2;
    }
    break;
  case RegexType::RegexHyperscan:
    if (!ssse3_support) {
      ENVOY_LOG(warn, "cpu do not support ssse3, use re2 instead");
      type_ = RegexType::RegexRe2;
    }
    break;
  case RegexType::RegexRe2:
    type_ = RegexType::RegexRe2;
    break;
  default:
    ENVOY_LOG(warn, "unsupported regex type: {}", type);
    break;
  }

  engines_.resize(EncodingType::MAX);
}

RegexMatcher::RegexMatcher(const std::string& db_path, RegexMode mode, RegexType type,
                           bool care_position)
    : RegexMatcher(mode, type, care_position) {
  if (!db_path.empty()) {
    for (int i = EncodingType::UTF8; i < EncodingType::MAX; i++) {
      db_path_[i] = db_path + "_encoding" + std::to_string(i) + ".sdb";
    }
  }
}

int RegexMatcher::init(const std::vector<ExpressionView>& expressions, std::vector<ExpressionView>& failed_exprs) {
  std::unique_lock<std::shared_mutex> lock(mtx_);
  if (type_ != RegexType::RegexHyperscan) {
    if (care_position_) {
      ENVOY_LOG(warn, "RegexType: {} position not support", type_);
      return -1;
    }

    if (mode_ != RegexMode::RegexModeBlock) {
      ENVOY_LOG(warn, "RegexType: {} non block mode not support", type_);
      return -1;
    }
  }

  if (expressions.size() == 0)
    return -1;

  // 保存传入的模式集
  for(auto it : expressions) {
    if (it.expr().length() == 0) {
      failed_exprs.push_back(it);
      expr_list_.clear();
      expr_map_.clear();
      return -1;
    }
    ExpressionViewPtr pExpr = std::make_shared<ExpressionView>(it);
    expr_list_.push_back(pExpr);
    if (expr_map_.end() == expr_map_.find(it.id())) {
      expr_map_[it.id()] = pExpr;
    } else {
      ENVOY_LOG(warn, "Expression: {}, id: {} exist!", pExpr->expr(), pExpr->id());
      failed_exprs.push_back(it);
      expr_list_.clear();
      expr_map_.clear();
      return -1;
    }
  }

  // 创建正则引擎
  std::vector<EnginePtr> engines(EncodingType::MAX, nullptr);
  int ret = createRegexEngine(failed_exprs, engines);
  if (ret == 0) {
    for (auto i = 0; i < EncodingType::MAX; i++) {
      engines_[i] = engines[i];
    }
  } else {
    expr_list_.clear();
    expr_map_.clear();
    return -1;
  }

  return 0;
}

int RegexMatcher::createRegexEngine(std::vector<ExpressionView>& failed_exprs, std::vector<EnginePtr> &engines) {
  // UTF8
  int ret = createRegexEncodingEngine(type_, expr_list_, failed_exprs, engines[EncodingType::UTF8], EncodingType::UTF8);
  if (ret != 0)
    return ret;

  // GBK
  std::vector<ExpressionViewPtr> gbk;
  for (const auto& it : expr_list_) {
    std::string val;
    if (RegexUtilities::convertImpl(it->expr(), val, "UTF-8", "GBK")) {
      gbk.emplace_back(std::make_shared<ExpressionView>(val, it->id(), it->ignore_case()));
    } else {
      ENVOY_LOG(error, "expression: {} convert to GBK failed", it->expr());
      return -1;
    }
  }
  ret = createRegexEncodingEngine(type_, gbk, failed_exprs, engines[EncodingType::GBK], EncodingType::GBK);
  if (ret != 0)
    return ret;

  // ISO
  return createRegexEncodingEngine(type_, expr_list_, failed_exprs, engines[EncodingType::ISO], EncodingType::ISO);
}

int RegexMatcher::createRegexEncodingEngine(RegexType type, const std::vector<ExpressionViewPtr>& expressions, std::vector<ExpressionView>& failed_exprs, EnginePtr &engine, EncodingType encode) {
  switch (type) {
  case RegexType::RegexHyperscan: {
    engine = std::make_shared<HyperscanEngine>(db_path_[encode], care_position_, static_cast<uint32_t>(mode_));
    break;
  }
  case RegexType::RegexRe2: {
    engine = std::make_shared<Re2Engine>(encode);
    break;
  }
  default:
    return -1;
  break;
  }

  return engine->init(expressions, failed_exprs);
}

int RegexMatcher::addExpression(const ExpressionView& expression)
{
  std::unique_lock<std::shared_mutex> lock(mtx_);
  if (expr_map_.end() != expr_map_.find(expression.id())) {
    return -1;
  }
  if (expression.expr().length() == 0) {
    return -1;
  }

  std::shared_ptr<ExpressionView> pExpr = std::make_shared<ExpressionView>(expression);
  expr_list_.push_back(pExpr);
  expr_map_[pExpr->id()] = pExpr;

  std::vector<ExpressionView> failed_exprs;
  std::vector<EnginePtr> engines(EncodingType::MAX, nullptr);
  int ret = createRegexEngine(failed_exprs, engines);
  if (ret == 0) {
    for (auto i = 0; i < EncodingType::MAX; i++) {
      engines_[i] = engines[i];
    }
  } else {
    ENVOY_LOG(error, "create engine failed!");
    // 恢复当前表
    expr_map_.erase(expression.id());
    expr_list_.pop_back();
    return -1;
  }
  return 0;
}

int RegexMatcher::delExpression(const uint32_t expr_id)
{
  std::unique_lock<std::shared_mutex> lock(mtx_);
  auto it = expr_map_.find(expr_id);
  if (it == expr_map_.end()) {
    return -1;
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
  std::vector<EnginePtr> engines(EncodingType::MAX, nullptr);
  int ret = createRegexEngine(failed_exprs, engines);
  if (ret == 0) {
    for (auto i = 0; i < EncodingType::MAX; i++) {
      engines_[i] = engines[i];
    }
  } else {
    ENVOY_LOG(error, "create engine failed!");
    // 恢复当前表
    expr_list_.push_back(pExpr);
    expr_map_[pExpr->id()] = pExpr;
    return -1;
  }

  return 0;
}

int RegexMatcher::match(const char* data, size_t len, std::vector<MatchResult>& results, EncodingType encode, int thread_index)
{
  return match(std::string_view(data, len), results, encode, thread_index);
}

int RegexMatcher::match(const std::string& data, std::vector<MatchResult>& results, EncodingType encode, int thread_index)
{
  return match(data.c_str(), data.length(), results, encode, thread_index);
}

int RegexMatcher::match(const std::string_view& data, std::vector<MatchResult>& results, EncodingType encode, int thread_index)
{
  if (data.length() == 0) {
    return 0;
  }
  
  if (mode_ != RegexMode::RegexModeBlock) {
    return -1;
  }

  std::shared_lock<std::shared_mutex> lock(mtx_);
  return engines_[encode]->match(data, results, thread_index);
}

int RegexMatcher::streamOpen(StreamContext& ctx, uint32_t stream_id, EncodingType encode) {
  if (mode_ != RegexMode::RegexModeStream || type_ != RegexType::RegexHyperscan) {
    return -1;
  }
  
  std::shared_lock<std::shared_mutex> lock(mtx_);
  HyperscanEnginePtr pHyperscan = std::dynamic_pointer_cast<HyperscanEngine>(engines_[encode]);
  ctx.encode = encode;
  return pHyperscan->streamOpen(ctx, stream_id);
}

int RegexMatcher::streamScan(StreamContext& ctx, const std::string_view& data, std::vector<MatchResult>& results, int thread_index) {
  if (mode_ != RegexMode::RegexModeStream || type_ != RegexType::RegexHyperscan) {
    return -1;
  }
  
  std::shared_lock<std::shared_mutex> lock(mtx_);
  HyperscanEnginePtr pHyperscan = std::dynamic_pointer_cast<HyperscanEngine>(engines_[ctx.encode]);
  return pHyperscan->streamScan(ctx, data, results, thread_index);
}

int RegexMatcher::streamClose(StreamContext& ctx, std::vector<MatchResult>& results, int thread_index) {
  if (mode_ != RegexMode::RegexModeStream || type_ != RegexType::RegexHyperscan) {
    return -1;
  }
  
  std::shared_lock<std::shared_mutex> lock(mtx_);
  HyperscanEnginePtr pHyperscan = std::dynamic_pointer_cast<HyperscanEngine>(engines_[ctx.encode]);
  return pHyperscan->streamClose(ctx, results, thread_index);
}

int RegexMatcher::vectorMatch(const std::vector<const char*>& data, const std::vector<uint32_t>& length, std::vector<MatchResult>& results, EncodingType encode, int thread_index) {
  if (mode_ != RegexMode::RegexModeVector || type_ != RegexType::RegexHyperscan) {
    return -1;
  }
  if (data.size() != length.size()) {
    return -1;
  }
  if (data.size() == 0) {
    return 0;
  }

  std::shared_lock<std::shared_mutex> lock(mtx_);
  HyperscanEnginePtr pHyperscan = std::dynamic_pointer_cast<HyperscanEngine>(engines_[encode]);
  return pHyperscan->match(data, length, results, thread_index);
}


std::shared_ptr<re2::RE2> RegexUtilities::EncodingRe(bool ignore_case, const std::string& utf8_pattern, EncodingType type) {
  re2::RE2::Options options;
  options.set_case_sensitive(!ignore_case);

  if (type == EncodingType::UTF8) {
    return std::make_shared<re2::RE2>(utf8_pattern, options);
  } else if (type == EncodingType::GBK) {
    // gbk
    std::string gbk;
    if (convertImpl(utf8_pattern, gbk, "UTF-8", "GBK")) {
      options.set_encoding(re2::RE2::Options::EncodingLatin1);
      return std::make_shared<re2::RE2>(gbk, options);
    }
  } else if (type == EncodingType::ISO) {
    if ((std::find_if(utf8_pattern.begin(), utf8_pattern.end(),
                      std::not1(std::ptr_fun(::isprint)))) == utf8_pattern.end()) {
      // same as utf8
    } else {
      options.set_encoding(re2::RE2::Options::EncodingLatin1);
    }
    return std::make_shared<re2::RE2>(utf8_pattern, options);
  } else {
    // error
  }

  return nullptr;
}

EncodingType RegexUtilities::getEncodeType(const std::string_view& content_type) {
  std::string str(content_type.data(), content_type.length());
  std::transform(str.begin(), str.end(), str.begin(), ::tolower);

  if (str.find("gbk") != std::string::npos) {
    return EncodingType::GBK;
  } else if (str.find("iso") != std::string::npos) {
    return EncodingType::ISO;
  }

  return EncodingType::UTF8;
}

bool RegexUtilities::convertImpl(const std::string_view& pattern, std::string& output,
                             const std::string_view& from_encode,
                             const std::string_view& to_encode) {
  iconv_t cd = iconv_open(to_encode.data(), from_encode.data());
  if (cd == reinterpret_cast<iconv_t>(-1)) {
    return false;
  }
  char* inbuf = const_cast<char*>(pattern.data());
  size_t inbytesleft = pattern.length();

  size_t capacity = inbytesleft * 4;
  size_t outbytesleft = capacity;
  std::vector<char> buf(outbytesleft);
  char* outptr = buf.data();

  bool res = true;
  while (inbytesleft > 0) {
    if (iconv(cd, &inbuf, &inbytesleft, &outptr, &outbytesleft) == static_cast<size_t>(-1)) {
      res = false;
      break;
    }
  }

  iconv_close(cd);
  if (res) {
    output.assign(buf.data(), capacity - outbytesleft);
  }

  return res;
}

RegexReplacer::RegexReplacer(bool ignore_case, const std::string& match_expr,
                             const std::string& replace_expr)
    : ignore_case_(ignore_case),
      match_expr_(match_expr),
      replace_expr_(replace_expr) {
  match_re2s_.resize(EncodingType::MAX);
  replace_exprs_.resize(EncodingType::MAX);
  for (int i = 0; i < EncodingType::MAX; i++) {
    match_re2s_[i] = RegexUtilities::EncodingRe(ignore_case_, match_expr_, static_cast<EncodingType>(i));
    if (!match_re2s_[i]) {
      ENVOY_LOG(error, "Encoding re2 regex failed: {}", match_expr_);
    }
  }

  replace_exprs_[EncodingType::UTF8] = replace_expr_;
  if (!RegexUtilities::convertImpl(replace_expr_, replace_exprs_[EncodingType::GBK], "UTF-8", "GBK")) {
    replace_exprs_[EncodingType::GBK] = "";
    ENVOY_LOG(error, "covert encoding gbk failed: {}", replace_expr_);
  }
  replace_exprs_[EncodingType::ISO] = replace_expr_;

}

bool RegexReplacer::replace(std::string& in, EncodingType code) {
  if (code >= EncodingType::MAX || !match_re2s_[code] || replace_exprs_[code].empty()) {
    return false;
  }

  return re2::RE2::Replace(&in, *match_re2s_[code], replace_exprs_[code]);
}

} // namespace RegexMatcher
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy