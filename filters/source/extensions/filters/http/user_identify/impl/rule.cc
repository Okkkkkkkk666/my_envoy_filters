#include "rule.h"

#include <algorithm>

#include "fmt/format.h"
namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace UserIdentify {
namespace Impl {

/**
 * 正则表达式写在 R"( 与 )"之间，可以不用转义
 * 运行时，会使用具体的关键字替换掉KEYWORD
 */
const RegexBlock UserNameRegex[ContentType::CONTENT_TYPE_MAX] = {
    {R"((^|[?&])KEYWORD=([^&]+))", 2},
    {R"("KEYWORD"[\s:]+"([^\n"]+)["])", 1},
    {R"(<KEYWORD>([^<^>]+)</KEYWORD>)", 1},
};

// token的长度最大为500个字节
const RegexBlock TokenRegex[TOKEN_TYPE_MAX] = {
    {R"("KEYWORD"[\s:]+"([^\n"]+)["])", 1},
    {R"((^|[;\s])KEYWORD=([^;^\r^\n]+)[;\r\n]?)", 2},
};

static std::string stringReplace(const std::string& orig, const std::string& from,
                                 const std::string& to) {
  std::string result;
  std::string::size_type pos = 0;
  do {
    auto n = orig.find(from, pos);
    if (n == std::string::npos) {
      result.append(orig.data() + pos);
      break;
    } else {
      result.append(orig.data() + pos, orig.data() + n);
      result.append(to);
      pos = n + from.size();
    }
  } while (pos < orig.size());

  return result;
}

static bool regex_extract(const std::string_view& data, const std::regex& re, size_t group,
                          std::string& result) {
  using MatchResults = std::match_results<std::string_view::const_iterator>;
  MatchResults m;
  if (std::regex_search(data.begin(), data.end(), m, re)) {
    // for (size_t i = 0; i < m.size(); i++) {
    //   std::cout << "index: " << i << " " << m[i].str() << std::endl;
    // }
    if (group < m.size()) {
      result = m[group].str();
      return true;
    }
  }

  return false;
}

CustomRule::CustomRule(const v3::Customization& customization, size_t id)
    : enabled_(customization.enabled()),
      cluster_name_(customization.cluster_name().cbegin(), customization.cluster_name().cend()),
      urls_(customization.url().cbegin(), customization.url().cend()),
      user_name_(customization.user_name().cbegin(), customization.user_name().cend()),
      common_info_name_(customization.common_info_name()), id_(id), url_match_str_([&]() {
        std::string merge_str;
        for (const auto& it : urls_) {
          merge_str.append(it);
          merge_str.append("|", 1);
        }
        std::transform(merge_str.begin(), merge_str.end(), merge_str.begin(), ::tolower);
        ENVOY_LOG(debug, "merge_str: {}", merge_str);
        return merge_str;
      }()) {
  // init username regex group
  user_name_regex_groups_.resize(ContentType::CONTENT_TYPE_MAX);
  for (auto i = 0; i < ContentType::CONTENT_TYPE_MAX; i++) {
    for (auto& it : user_name_) {
      RegexBlock block;
      block.regex_str = stringReplace(UserNameRegex[i].regex_str, "KEYWORD", it);
      block.extract_group = UserNameRegex[i].extract_group;
      block.re = std::regex(block.regex_str, std::regex_constants::icase);
      user_name_regex_groups_[i].emplace_back(block);
    }
  }
  // init username regex matchers
  user_name_regex_matchers_.resize(ContentType::CONTENT_TYPE_MAX);
  for (auto i = 0; i < ContentType::CONTENT_TYPE_MAX; i++) {
    user_name_regex_matchers_[i] = IdentifyBase::regexMatcherInit(user_name_regex_groups_[i]);
  }
}

bool CustomRule::matchUrl(const std::string_view& url) const {
  std::string lower_url(url);
  std::transform(lower_url.begin(), lower_url.end(), lower_url.begin(), ::tolower);
  if (std::string_view::npos != url_match_str_.find(lower_url)) {
    return true;
  }
  return false;
}

bool CustomRule::realGetUserName(const std::string_view& data_view, ContentType type, size_t id,
                                 std::string& user_name) {
  const RegexGroup& group = user_name_regex_groups_[type];
  if (id >= group.size()) {
    return false;
  }
  const RegexBlock& block = group[id];
  return regex_extract(data_view, block.re, block.extract_group, user_name);
}

bool CustomRule::getUserNameByType(const std::string_view& data_view, ContentType type,
                                   int thread_index, std::string& user_name) {
  RegexMatcherPtr matcher = user_name_regex_matchers_[type];
  if (!matcher || data_view.empty()) {
    return false;
  }

  // 扫描
  std::vector<RegexMatcher::MatchResult> results;
  if (0 != matcher->match(data_view, results, RegexMatcher::EncodingType::UTF8, thread_index)) {
    return false;
  }
  if (results.empty()) {
    return false;
  }

  // 只取第一个
  ENVOY_LOG(trace, "id: {}, start: {}, end: {}", results[0].id(), results[0].start(),
            results[0].end());
  std::string_view match_piece(data_view.data() + results[0].start(),
                               results[0].end() - results[0].start());
  return realGetUserName(match_piece, type, results[0].id(), user_name);
}

IdentifyBase::IdentifyBase(const std::string& url_list, const std::string& login_token_list,
                           const std::string& guest_token_list)
    : urls_([&]() { return absl::StrSplit(url_list, "\r\n"); }()),
      login_tokens_([&]() { return absl::StrSplit(login_token_list, "\r\n"); }()),
      guest_tokens_([&]() { return absl::StrSplit(guest_token_list, "\r\n"); }()) {
  // init url_regex matchers
  url_regex_matcher_ = regexMatcherInit(urls_);
  if (!url_regex_matcher_) {
    ENVOY_LOG(error, "url_regex_matcher_ init failed");
  }

  // init token matchers;
  ENVOY_LOG(trace, "login_token_list: {}", login_token_list);
  ENVOY_LOG(trace, "guest_token_list: {}", guest_token_list);
  token_matchers_.resize(HTTP_TYPE_MAX);
  token_matchers_[HTTP_TYPE_LOGIN] =
      initMatcherBlock("token_matchers_login", login_tokens_, TOKEN_TYPE_MAX);
  token_matchers_[HTTP_TYPE_GUEST] =
      initMatcherBlock("token_matchers_guest", guest_tokens_, TOKEN_TYPE_MAX);
}

RegexMatcherPtr
IdentifyBase::regexMatcherInit(const std::vector<RegexMatcher::ExpressionView>& exprs) {
  // init regex matcher
  RegexMatcherPtr matcher = std::make_shared<RegexMatcher::RegexMatcher>(
      RegexMatcher::RegexType::RegexHyperscan, true, false);
  std::vector<RegexMatcher::ExpressionView> failed_exprs;
  int ret = matcher->init(exprs, failed_exprs);
  if (ret < 0) {
    ENVOY_LOG(trace, "regex matcher init failed: {}", ret);
    for (auto& it : failed_exprs) {
      ENVOY_LOG(error, "expression id: {}, {} failed", it.id(), it.expr());
    }
    return nullptr;
  }

  ENVOY_LOG(trace, "regex matcher init success");
  return matcher;
}

RegexMatcherPtr IdentifyBase::regexMatcherInit(const RegexGroup& regex_group) {
  std::vector<RegexMatcher::ExpressionView> exprs;
  for (size_t i = 0; i < regex_group.size(); i++) {
    exprs.emplace_back(RegexMatcher::ExpressionView(regex_group[i].regex_str, i, true));
    ENVOY_LOG(trace, "add pattern: {}, id: {}", regex_group[i].regex_str, i);
  }
  return regexMatcherInit(exprs);
}

RegexMatcherPtr IdentifyBase::regexMatcherInit(const std::vector<std::string>& regex_exprs) {
  std::vector<RegexMatcher::ExpressionView> exprs;
  for (size_t i = 0; i < regex_exprs.size(); i++) {
    exprs.emplace_back(RegexMatcher::ExpressionView(regex_exprs[i], i, true));
    ENVOY_LOG(trace, "add pattern: {}, id: {}", regex_exprs[i], i);
  }
  return regexMatcherInit(exprs);
}

MatcherBlockPtr IdentifyBase::initMatcherBlock(const std::string& matcher_name,
                                               const std::vector<std::string>& keys,
                                               int group_size) {
  MatcherBlockPtr matcher_block = std::make_shared<MatcherBlock>();

  matcher_block->regex_groups_.resize(group_size);
  for (auto i = 0; i < group_size; i++) {
    for (auto& it : keys) {
      RegexBlock block;
      block.regex_str = stringReplace(TokenRegex[i].regex_str, "KEYWORD", it);
      block.extract_group = TokenRegex[i].extract_group;
      block.re = std::regex(block.regex_str, std::regex_constants::icase);
      matcher_block->regex_groups_[i].emplace_back(block);
    }
  }
  matcher_block->regex_matchers_.resize(group_size);
  for (auto i = 0; i < group_size; i++) {
    matcher_block->regex_matchers_[i] =
        IdentifyBase::regexMatcherInit(matcher_block->regex_groups_[i]);
    if (matcher_block->regex_matchers_[i]) {
      ENVOY_LOG(trace, "{} regex_matcher init success", matcher_name);
    }
  }

  ENVOY_LOG(trace, "{} matcher_block init success", matcher_name);

  return matcher_block;
}

bool IdentifyBase::isLoginByDefault(const std::string_view& url, const IdentifyCtxPtr ctx) {
  if (!url_regex_matcher_ || url.empty()) {
    return false;
  }

  // 扫描
  std::vector<RegexMatcher::MatchResult> results;
  if (0 != url_regex_matcher_->match(url, results, RegexMatcher::EncodingType::UTF8,
                                     ctx->thread_index)) {
    return false;
  }
  if (results.empty()) {
    return false;
  }

  return true;
}

bool IdentifyBase::realGetToken(const RegexGroup& regex_group, const std::string_view& data_view,
                                size_t id, std::string& token) const {
  if (id >= regex_group.size()) {
    return false;
  }
  const RegexBlock& block = regex_group[id];
  return regex_extract(data_view, block.re, block.extract_group, token);
}

bool IdentifyBase::getTokenByType(const std::string_view& data_view, const IdentifyCtxPtr ctx,
                                  HttpType http_type, TokenRegexType token_type,
                                  std::string& token) const {
  std::string user_name;
  MatcherBlockPtr matcher = token_matchers_[http_type];
  if (!matcher || data_view.empty()) {
    return false;
  }
  RegexMatcherPtr regex_matcher = matcher->regex_matchers_[token_type];

  if (!regex_matcher) {
    return false;
  }

  // 扫描
  std::vector<RegexMatcher::MatchResult> results;
  if (0 != regex_matcher->match(data_view, results, RegexMatcher::EncodingType::UTF8,
                                ctx->thread_index)) {
    return false;
  }
  if (results.empty()) {
    return false;
  }

  // 只取第一个
  ENVOY_LOG(trace, "id: {}, start: {}, end: {}", results[0].id(), results[0].start(),
            results[0].end());
  std::string_view match_piece(data_view.data() + results[0].start(),
                               results[0].end() - results[0].start());
  return realGetToken(matcher->regex_groups_[token_type], match_piece, results[0].id(), token);
}

bool IdentifyBase::getTokenFromCookie(const std::string_view& data, const IdentifyCtxPtr ctx,
                                      HttpType http_type, std::string& token) const {
  if (!ctx) {
    return false;
  }
  return getTokenByType(data, ctx, http_type, TOKEN_TYPE_COOKIE, token);
}

bool IdentifyBase::getTokenFromBody(const std::string_view& data, const IdentifyCtxPtr ctx,
                                    HttpType http_type, TokenRegexType token_type,
                                    std::string& token) const {
  if (!ctx) {
    return false;
  }
  return getTokenByType(data, ctx, http_type, token_type, token);
}

Customize::Customize(const v3::Customize& customize, const std::string& url_list,
                     const std::string& login_token_list, const std::string& guest_token_list)
    : IdentifyBase(url_list, login_token_list, guest_token_list) {
  int id = 0;
  for (const auto& it : customize.customization()) {
    rules_.emplace_back(std::make_shared<CustomRule>(it, id++));
  }

  for (const auto& it : rules_) {
    ENVOY_LOG(trace, "id:{}, enable:{}, cluster_name:{}, url:{}, user_name:{}, common_info_name:{}",
              it->id(), it->enabled(), it->cluster_name().size(), it->urls().size(),
              it->user_name().size(), it->common_info_name());
    if (!it->enabled()) {
      continue;
    }

    // create cluster_name map
    for (const auto& cluster_name : it->cluster_name()) {
      if (cluster_map_.end() != cluster_map_.find(cluster_name)) {
        ENVOY_LOG(warn, "cluster name duplicate at id: {}, common_info_name: {}, ignore", it->id(),
                  it->common_info_name());
        continue;
      }

      cluster_map_[cluster_name] = it->id();
    }
  }
}

bool Customize::initCtx(const std::string& cluster_name, int thread_index, IdentifyCtxPtr ctx) {
  ctx->rule_index = -1;
  ctx->thread_index = thread_index;

  auto it = cluster_map_.find(cluster_name);
  if (it == cluster_map_.end()) {
    return false;
  }
  size_t id = it->second;
  if (id >= rules_.size()) {
    ENVOY_LOG(error, "id is invalid: {}", id);
    return false;
  }
  ctx->rule_index = id;
  return true;
}

bool Customize::isLogin(const std::string_view& url, const IdentifyCtxPtr ctx) {
  if (!ctx || ctx->rule_index < 0) {
    ENVOY_LOG(error, "id is invalid");
    return false;
  }

  CustomRulePtr rule = rules_[ctx->rule_index];
  if (rule->urls().size()) {
    return rule->matchUrl(url);
  } else {
    return isLoginByDefault(url, ctx);
  }
}

bool Customize::getUserNameFromUrl(const std::string_view& url, const IdentifyCtxPtr ctx,
                                   std::string& user_name) {
  if (!ctx || ctx->rule_index < 0) {
    ENVOY_LOG(error, "id is invalid");
    return false;
  }

  CustomRulePtr rule = rules_[ctx->rule_index];
  return rule->getUserNameByType(url, ContentType::CONTENT_TYPE_FROM_URLENCODED, ctx->thread_index,
                                 user_name);
}

bool Customize::getUserNameFromBody(const std::string_view& data, const IdentifyCtxPtr ctx,
                                    ContentType type, std::string& user_name) {
  if (!ctx || ctx->rule_index < 0) {
    ENVOY_LOG(error, "id is invalid");
    return false;
  }

  CustomRulePtr rule = rules_[ctx->rule_index];
  return rule->getUserNameByType(data, type, ctx->thread_index, user_name);
}

Auto::Auto(const std::string& user_name_list, const std::string& url_list,
           const std::string& login_token_list, const std::string& guest_token_list)
    : IdentifyBase(url_list, login_token_list, guest_token_list),
      user_name_list_([&]() { return absl::StrSplit(user_name_list, "\r\n"); }()) {
  // init username regex group
  user_name_regex_groups_.resize(ContentType::CONTENT_TYPE_MAX);
  for (auto i = 0; i < ContentType::CONTENT_TYPE_MAX; i++) {
    for (auto& it : user_name_list_) {
      RegexBlock block;
      block.regex_str = stringReplace(UserNameRegex[i].regex_str, "KEYWORD", it);
      block.extract_group = UserNameRegex[i].extract_group;
      block.re = std::regex(block.regex_str, std::regex_constants::icase);
      user_name_regex_groups_[i].emplace_back(block);
    }
  }
  // init username regex matchers
  user_name_regex_matchers_.resize(ContentType::CONTENT_TYPE_MAX);
  for (auto i = 0; i < ContentType::CONTENT_TYPE_MAX; i++) {
    user_name_regex_matchers_[i] = IdentifyBase::regexMatcherInit(user_name_regex_groups_[i]);
  }
}

bool Auto::isLogin(const std::string_view& url, const IdentifyCtxPtr ctx) {
  if (!ctx) {
    ENVOY_LOG(error, "ctx nullptr");
    return false;
  }
  return isLoginByDefault(url, ctx);
}

bool Auto::initCtx(const std::string&, int thread_index, IdentifyCtxPtr ctx) {
  ctx->rule_index = -1;
  ctx->thread_index = thread_index;
  return true;
}

bool Auto::getUserNameByType(const std::string_view& data_view, ContentType type, int thread_index,
                             std::string& user_name) {
  RegexMatcherPtr matcher = user_name_regex_matchers_[type];
  if (!matcher || data_view.empty()) {
    return false;
  }

  // 扫描
  std::vector<RegexMatcher::MatchResult> results;
  if (0 != matcher->match(data_view, results, RegexMatcher::EncodingType::UTF8, thread_index)) {
    return false;
  }
  if (results.empty()) {
    return false;
  }
  // 只取第一个
  ENVOY_LOG(trace, "id: {}, start: {}, end: {}", results[0].id(), results[0].start(),
            results[0].end());
  std::string_view match_piece(data_view.data() + results[0].start(),
                               results[0].end() - results[0].start());
  const RegexGroup group = user_name_regex_groups_[type];
  if (results[0].id() >= group.size()) {
    return false;
  }
  const RegexBlock& block = group[results[0].id()];
  return regex_extract(match_piece, block.re, block.extract_group, user_name);
}

bool Auto::getUserNameFromUrl(const std::string_view& url, const IdentifyCtxPtr ctx,
                              std::string& user_name) {
  if (!ctx) {
    ENVOY_LOG(error, "id is nullptr");
    return false;
  }
  return getUserNameByType(url, ContentType::CONTENT_TYPE_FROM_URLENCODED, ctx->thread_index,
                           user_name);
}

bool Auto::getUserNameFromBody(const std::string_view& data, const IdentifyCtxPtr ctx,
                               ContentType type, std::string& user_name) {
  if (!ctx) {
    ENVOY_LOG(error, "id is nullptr");
    return false;
  }
  return getUserNameByType(data, type, ctx->thread_index, user_name);
}

} // namespace Impl
} // namespace UserIdentify
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy