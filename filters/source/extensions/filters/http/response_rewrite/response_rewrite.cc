#include "response_rewrite.h"
#include "filters/source/extensions/filters/http/common/utility/utility.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace ResponseRewrite {

using namespace Envoy::Extensions::Filters::Common::Utility;
const std::string ResponseRewrite::filter_name_(FILTER_NAME);

RewriteRule::RewriteRule(const v3::RewriteRule& rewrite)
    : sensitive_type_(rewrite.sensitive_type()) {
  switch (rewrite.rewrite_method_case()) {
  case v3::RewriteRule::kEncryptAlgorithmRewrite:
    encrypt_algorithmPtr_ =
        std::make_unique<Algorithm::EncryptAlgorithmRewrite>(rewrite.encrypt_algorithm_rewrite());
    break;
  case v3::RewriteRule::kHashEncryptRewrite:
    hash_encryptPtr_ =
        std::make_unique<Algorithm::HashEncryptRewrite>(rewrite.hash_encrypt_rewrite());
    break;
  case v3::RewriteRule::kShuffleRewrite:
    shufflePtr_ = std::make_unique<Algorithm::ShuffleRewrite>(rewrite.shuffle_rewrite());
    break;
  case v3::RewriteRule::kCoverRewrite:
    coverPtr_ = std::make_unique<Algorithm::CoverRewrite>(rewrite.cover_rewrite());
    break;
  case v3::RewriteRule::kReplaceRewrite:
    replacePtr_ = std::make_unique<Algorithm::ReplaceRewrite>(rewrite.replace_rewrite());
    break;
  case v3::RewriteRule::kTransformRewrite:
    transformPtr_ = std::make_unique<Algorithm::TransformRewrite>(rewrite.transform_rewrite());
    break;
  default:
    break;
  }
}

std::string RewriteRule::rewrite(const std::string& data, RegexMatcher::EncodingType& encode_type) {
  std::string result;
  if (transformPtr_) {
    result = transformPtr_->transForm(data, encode_type);
    return result;
  }
  if (hash_encryptPtr_) {
    result = hash_encryptPtr_->hashEncrypt(data);
    return result;
  }
  if (encrypt_algorithmPtr_) {
    result = encrypt_algorithmPtr_->encryptAlgorithm(data);
    return result;
  }
  if (coverPtr_) {
    result = coverPtr_->coverData(data, encode_type);
    return result;
  }
  if (replacePtr_) {
    result = replacePtr_->execudata(data, encode_type);
    return result;
  }
  result = shufflePtr_->shuffle(data, encode_type);
  return result;
}

IdentifyRule::IdentifyRule(const v3::IdentifyRule& identify)
    : sensitive_type_name_(identify.sensitive_type()), type_(identify.identify_type()),
      logic_(identify.recognition_logic()), field_name_(identify.field_name()),
      data_content_(identify.data_content()) {}

Rule::Rule(const v3::Rule& rule)
    : matchers_([&rule]() {
        std::vector<Filters::Common::StrongMatcher::Matcher> result;
        for (const envoy::extensions::filters::http::common::strong_matcher::v3::Matcher& matcher :
             rule.match()) {
          result.emplace_back(matcher);
        }
        return result;
      }()),
      id_(rule.id()), enable_(rule.enable()),
      rewrite_rule_(std::make_unique<RewriteRule>(rule.rewrite_rule())),
      identify_rule_(std::make_unique<IdentifyRule>(rule.identify_rule())),
      rewrite_strategy_name_(rule.rewrite_strategy_name()) {}

bool Rule::match(const Network::Address::InstanceConstSharedPtr& address,
                 const Http::RequestHeaderMap& headers, const std::string& username) const {
  for (auto& matcher : matchers_) {
    if (!matcher.matchAll(headers, address, username)) {
      return false;
    }
  }
  return true;
}

ResponseRewriteRouteConfig::ResponseRewriteRouteConfig(
    const v3::ResponseRewritePerRoute& proto_config)
    : enable_(proto_config.enable()),
      status_codes_(proto_config.status_codes().begin(), proto_config.status_codes().end()),
      buffer_(proto_config.buffer()), compressor_enable_(proto_config.compressor_enable()),
      white_list_(proto_config.whitelist()), rules_([&proto_config]() {
        std::vector<std::unique_ptr<Rule>> rules;
        for (auto& rule : proto_config.rules()) {
          if (rule.enable()) {
            rules.emplace_back(std::make_unique<Rule>(rule));
          }
        }
        std::sort(rules.begin(), rules.end(), compareRulesById);
        return rules;
      }()) {
  for (auto& rule : rules_) {
    for (auto& match : rule->matcher()) {
      for (auto& user : match.getUserMatchers()) {
        username_ = user->username();
      }
    }
  }
  std::vector<RegexMatcher::ExpressionView> exprs;
  matcher_ = std::make_shared<RegexMatcher::RegexMatcher>(RegexMatcher::RegexType::RegexHyperscan,
                                                          true, false);
  for (size_t i = 0; i < rules_.size(); i++) {
    auto identify_rule = rules_[i]->identifyRule();
    auto result =
        executeIdentifyRule(identify_rule->identifyType(), identify_rule->recognitionLogic(),
                            identify_rule->dataContent(), identify_rule->fieldName());
    exprs.emplace_back(result, i, true);
  }
  std::vector<RegexMatcher::ExpressionView> failed_exprs;
  int ret = matcher_->init(exprs, failed_exprs);
  if (ret < 0) {
    ENVOY_LOG(trace, "regex matcher init failed: {}", ret);
    for (auto& it : failed_exprs) {
      ENVOY_LOG(error, "expression id: {}, {} failed", it.id(), it.expr());
    }
    matcher_ = nullptr;
  }
  ENVOY_LOG(trace, "regex matcher init success");
}

bool ResponseRewriteRouteConfig::compareRulesById(std::unique_ptr<Rule>& lhs,
                                                  std::unique_ptr<Rule>& rhs) {
  return lhs->id() < rhs->id();
}

IpWhitelist::IpWhitelist(const v3::IpWhitelist& ipwhitelist)
    : enable_(ipwhitelist.enable()), sensitive_type_(ipwhitelist.sensitive_type()),
      sensitive_type_all_(ipwhitelist.sensitive_type_all()), ip_whitelist_([&ipwhitelist]() {
        std::vector<Filters::Common::IpWhitelist::IpWhitelistPtr> ip_whitelists;
        for (const auto& ip_whitelist : ipwhitelist.ip_whitelist()) {
          ip_whitelists.push_back(
              std::make_unique<Filters::Common::IpWhitelist::IpWhitelist>(ip_whitelist));
        }
        return ip_whitelists;
      }()) {}

bool IpWhitelist::matchIp(const Network::Address::InstanceConstSharedPtr& address) {
  for (auto& ip_whitelist : ip_whitelist_) {
    if (ip_whitelist->match(address)) {
      return true;
    }
  }
  return false;
}

UserWhitelist::UserWhitelist(const v3::UserWhitelist& userwhitelist)
    : enable_(userwhitelist.enable()), sensitive_type_(userwhitelist.sensitive_type()),
      sensitive_type_all_(userwhitelist.sensitive_type_all()), user_whitelist_([&userwhitelist]() {
        std::vector<Filters::Common::StrongMatcher::UserMatcherptr> user_whitelists;
        for (const auto& user : userwhitelist.user()) {
          user_whitelists.push_back(
              std::make_unique<Filters::Common::StrongMatcher::UserMatcher>(user));
        }
        return user_whitelists;
      }()) {}

bool UserWhitelist::matchUsername(const std::string& username) {
  for (auto& user : user_whitelist_) {
    if (user->matches(username)) {
      return true;
    }
  }
  return false;
}

Whitelist::Whitelist(const v3::Whitelist& whitelist)
    : ip_white_list_([&whitelist]() {
        std::vector<std::shared_ptr<IpWhitelist>> ip_whitelists;
        for (auto& ip : whitelist.ip_whitelist()) {
          ip_whitelists.emplace_back(std::make_shared<IpWhitelist>(ip));
        }
        return ip_whitelists;
      }()),
      user_white_list_([&whitelist]() {
        std::vector<std::shared_ptr<UserWhitelist>> user_whitelists;
        for (auto& user : whitelist.user_whitelist()) {
          user_whitelists.emplace_back(std::make_shared<UserWhitelist>(user));
        }
        return user_whitelists;
      }()) {}

// 将','转为'|'
static std::string replaceCommasWithPipes(const std::string& input) {
  std::string result = input;
  size_t pos = 0;
  while ((pos = result.find(',', pos)) != std::string::npos) {
    result[pos] = '|';
    pos++;
  }
  return result;
}
// 替换key值
static std::string replaceKey(const std::string& key) {
  std::string prefix = R"R(("(KEYWORD)"[\s:]+"([^\n"]+)["])|("(KEYWORD)"[\s:]+([^\n"](-?\d+)(\.\d+)?)))R";
  std::string::size_type pos = 0;
  while ((pos = prefix.find("KEYWORD", pos)) != std::string::npos) {
    prefix.replace(pos, 7, key);
    pos += key.size();
  }
  return prefix;
}

// 替换key和value
static std::string replaceKeyValue(const std::string& key, const std::string& value) {
  std::string prefix = R"R(("(KEYWORD)"[\s:]+"(VALUE)")|("(KEYWORD)"[\s:]+(VALUE)))R";

  std::string::size_type key_pos = 0;
  while ((key_pos = prefix.find("KEYWORD", key_pos)) != std::string::npos) {
    prefix.replace(key_pos, 7, key);
    key_pos += key.size();
  }

  std::string::size_type value_pos = 0;
  while ((value_pos = prefix.find("VALUE", value_pos)) != std::string::npos) {
    prefix.replace(value_pos, 5, value);
    value_pos += value.size();
  }

  return prefix;
}

static std::vector<std::string> splitString(const std::string& str) {
  std::vector<std::string> tokens;
  std::stringstream ss(str);
  std::string token;
  while (std::getline(ss, token, ',')) {
    if (!token.empty()) {
      tokens.push_back(token);
    }
  }
  return tokens;
}

bool ResponseRewriteRouteConfig::matchStatusCode(uint32_t status_code) const {
  // 如果配置了状态码则匹配状态码
  if (status_codes_.empty()) {
    return true;
  }

  return status_codes_.find(status_code) != status_codes_.end();
}

std::string ResponseRewriteRouteConfig::executeIdentifyRule(
    const v3::IdentifyRule_IdentifyType identify_type,
    const v3::IdentifyRule_RecognitionLogic recognition_logic, const std::string& data_content,
    const std::string& field_name) {
  std::string rules;
  if (identify_type == v3::IdentifyRule_IdentifyType::IdentifyRule_IdentifyType_DATA_CONTENT) {
    rules = data_content;
  } else if (identify_type == v3::IdentifyRule_IdentifyType::IdentifyRule_IdentifyType_FIELD_NAME) {
    auto result = replaceCommasWithPipes(field_name);
    rules = replaceKey(result);
  } else if (identify_type ==
             v3::IdentifyRule_IdentifyType::IdentifyRule_IdentifyType_DATA_CONTENT_AND_FIELD_NAME) {
    if (recognition_logic ==
        v3::IdentifyRule_RecognitionLogic::IdentifyRule_RecognitionLogic_SATISFY_ALL) {
      auto result = replaceCommasWithPipes(field_name);
      rules = replaceKeyValue(result, data_content);
    } else if (recognition_logic ==
               v3::IdentifyRule_RecognitionLogic::IdentifyRule_RecognitionLogic_SATISFY_ANY) {
      auto result = replaceCommasWithPipes(field_name);
      rules = replaceKey(result);
      rules =  rules + "|" + data_content;
    }
  }
  return rules;
}

int ResponseRewrite::threadIndex() {
  if (thread_index_ != -1)
    return thread_index_;

  // worker_10
  std::string thread_name;
  if (encoder_callbacks_) {
    thread_name = encoder_callbacks_->dispatcher().name();
  } else if (decoder_callbacks_) {
    thread_name = decoder_callbacks_->dispatcher().name();
  } else {
    return -1;
  }

  auto pos = thread_name.find_first_of('_');
  if (pos == std::string::npos) {
    ENVOY_LOG(error, "Parse thread index failed: {}", thread_name);
    return -1;
  } else {
    thread_index_ = std::atol(thread_name.data() + pos + 1);
  }
  return thread_index_;
}

Http::FilterHeadersStatus ResponseRewrite::decodeHeaders(Http::RequestHeaderMap& headers, bool) {
  const Envoy::Router::VirtualHostImpl* vh = getVirtualHost();
  const ResponseRewriteRouteConfig* filter_vh_config = getVirtualHostConfig(vh);

  // 获取路由配置
  const ResponseRewriteRouteConfig* filter_route_config = getRouteConfig(filter_vh_config);
  // 获取IP、上游集群名
  const Network::Address::InstanceConstSharedPtr downstreamAddress = getDownstreamAddress();

  std::string username;
  // 判断是否存在用户匹配
  if (filter_route_config != nullptr) {
    if (!filter_route_config->username().empty()) {
      if (!getUserInfo(username)) {
        if (headers.ContentLength() != nullptr) {
          // 保存请求头
          saved_headers_ = &headers;
          ENVOY_LOG(debug, "not found username");
          return Http::FilterHeadersStatus::Continue;
        }
      }
    }
  } else if (filter_vh_config != nullptr) {
    if (!filter_vh_config->username().empty()) {
      if (!getUserInfo(username)) {
        if (headers.ContentLength() != nullptr) {
          saved_headers_ = &headers;
          ENVOY_LOG(debug, "not found username");
          return Http::FilterHeadersStatus::Continue;
        }
      }
    }
  }

  matchCollectRules(filter_vh_config, filter_route_config, downstreamAddress, headers, username);

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus ResponseRewrite::decodeData(Buffer::Instance&, bool) {
  // 判断请求头是否存在
  if (saved_headers_ == nullptr) {
    return Http::FilterDataStatus::Continue;
  }
  std::string username;
  if (!getUserInfo(username)) {
    ENVOY_LOG(debug, "not found username");
  }
  // 判断VH中是否存在配置
  const Envoy::Router::VirtualHostImpl* vh = getVirtualHost();
  const ResponseRewriteRouteConfig* filter_vh_config = getVirtualHostConfig(vh);

  // 获取路由配置
  const ResponseRewriteRouteConfig* filter_route_config = getRouteConfig(filter_vh_config);
  // 获取IP、上游集群名
  const Network::Address::InstanceConstSharedPtr downstreamAddress = getDownstreamAddress();

  matchCollectRules(filter_vh_config, filter_route_config, downstreamAddress, *saved_headers_,
                    username);

  return Http::FilterDataStatus::Continue;
}

Http::FilterHeadersStatus ResponseRewrite::encodeHeaders(Http::ResponseHeaderMap& headers,
                                                         bool end_stream) {
  if (end_stream) {
    return Http::FilterHeadersStatus::Continue;
  }

  if (!isNeedRewrite(headers)) {
    return Http::FilterHeadersStatus::Continue;
  }

  headers_ = &headers;
  return Http::FilterHeadersStatus::StopIteration;
}

Http::FilterDataStatus ResponseRewrite::encodeData(Buffer::Instance& data, bool end_stream) {
  if (!headers_) {
    return Http::FilterDataStatus::Continue;
  }

  PROCESS_ENCODER_BUFFER_LIMIT(is_encode_buffer_full_);

  encoder_callbacks_->modifyEncodingBuffer([this](Buffer::Instance& enc_buf) {
    Envoy::Buffer::OwnedImpl buf;

    if (decompressor_) {
      // 解压缩
      buf.move(enc_buf);
      decompressor_->decompress(buf, enc_buf);
    }

    std::string_view data_view(static_cast<char*>(enc_buf.linearize(enc_buf.length())),
                               enc_buf.length());

    // 扫描
    std::vector<RegexMatcher::MatchResult> results;
    RegexMatcherPtr regex_matcher = filter_config_->matcher();
    if (regex_matcher) {
      if (0 != regex_matcher->match(data_view, results, encode_type_, threadIndex())) {
        results.clear();
        ENVOY_LOG(error, "match failed!");
      }
    }

    // 脱敏操作
    log_.set_act(v3::ResponseRewriteLog_Action_PASS);
    if (!results.empty()) {
      buf.drain(buf.length());
      // rewrite enc_buf -> buf
      doRewrite(results, data_view, buf);

      // move buf -> enc_buf
      enc_buf.drain(enc_buf.length());
      enc_buf.move(buf);
    }

    if (comperssor_) {
      // 压缩
      comperssor_->compress(enc_buf, Envoy::Compression::Compressor::State::Finish);
    }
    headers_->setContentLength(enc_buf.length());
  });

  return Http::FilterDataStatus::Continue;
}

bool ResponseRewrite::isNeedRewrite(Http::ResponseHeaderMap& headers) {
  bool result = false;

  do {
    // 检查配置
    filter_config_ = getConfig();
    if (!filter_config_) {
      break;
    }
    // 检查符合脱敏条件的规则是否为空
    if (id_.empty()) {
      break;
    }

    // 获取响应码
    auto status_code = strtoul(headers.getStatusValue().data(), nullptr, 10);
    if (!filter_config_->matchStatusCode(status_code)) {
      break;
    }

    const auto content_type = headers.getContentTypeValue();
    encode_type_ = RegexMatcher::RegexUtilities::getEncodeType(std::string_view(content_type.data(), content_type.length()));

    Envoy::Http::HeaderMap::GetResult get_result =
        headers.get(Http::CustomHeaders::get().ContentEncoding);
    if (!get_result.empty()) {
      if (!filter_config_->compressor_enable()) {
        // 为不启用解压缩功能, 不能脱敏
        break;
      }

      auto content_encoding = get_result[0]->value().getStringView();
      if (content_encoding.find("gzip") != std::string::npos) {
        decompressor_ = filter_hcm_config_->makeDecompressorGzip();
        comperssor_ = filter_hcm_config_->makeCompressorGzip();
      } else if (content_encoding.find("br") != std::string::npos) {
        decompressor_ = filter_hcm_config_->makeDecompressorBrotli();
        comperssor_ = filter_hcm_config_->makeCompressorBrotli();
      } else {
        // 不支持的压缩格式，不能脱敏
        break;
      }
    }

    result = true;
  } while (0);

  return result;
}

static std::string extractValue(const std::string& data) {
  std::regex valueRegex(R"R(":\s*"([^"]*)"|:\s*([^,}\]]*))R");
  std::smatch match;
  if (std::regex_search(data, match, valueRegex)) {
    if (match.size() > 1 && !match.str(1).empty()) {
      return match.str(1);
    } else if (match.size() > 2) {
      return match.str(2);
    }
  }
  return data;
}

static std::string replaceValue(const std::string& source, const std::string& new_value) {
  size_t start = source.find('"');
  if (start == std::string::npos)
    return new_value;

  size_t end = source.find('"', start + 1);
  if (end == std::string::npos)
    return new_value;

  size_t value_start = source.find('"', end + 1);
  if (value_start == std::string::npos)
    return source.substr(0, end + 2) + new_value;

  size_t value_end = source.find('"', value_start + 1);
  if (value_end == std::string::npos)
    return new_value;

  std::string result = source.substr(0, value_start + 1) + new_value + source.substr(value_end);

  return result;
}

void ResponseRewrite::doRewrite(std::vector<RegexMatcher::MatchResult>& records,
                                const std::string_view& data_buf_in,
                                Envoy::Buffer::OwnedImpl& data_buf_out) {

  const char* pdata = data_buf_in.data();
  std::vector<std::string> new_data;
  size_t pos = 0;
  uint32_t new_length = 0;
  const auto& rules = filter_config_->rules();
  uint32_t rewrite_count = 0;

  time_t time_now = time(0);

  // 排序
  std::sort(records.begin(), records.end(), RegexMatcher::MatchResult::cmp);
  //  解决冲突
  int sz = records.size();
  log_.set_act(v3::ResponseRewriteLog_Action_REWRITE);
  int j = 0;
  for (int i = 1; i < sz; ++i) {
    if (records[i].start() < records[j].end()) {
      if (records[i].id() < records[j].id()) {
        records[j].set_flag(false);
        j = i;
      } else {
        records[i].set_flag(false);
      }
    } else {
      j = i;
    }
  }

  for (const auto& item : records) {
    if (item.flag()) {
      auto rew = rules[item.id()]->rewriteRule();
      auto identify = rules[item.id()]->identifyRule();
      auto iter = std::find(std::begin(id_), std::end(id_), item.id());
      if (iter != std::end(id_)) {
        new_data.emplace_back(std::string(pdata + item.start(), item.end() - item.start()));
        auto value = extractValue(new_data.back());
        new_data.back() = replaceValue(new_data.back(), rew->rewrite(value, encode_type_));
        auto p = log_.mutable_matched_patterns()->Add();
        // 敏感类型名称
        p->set_sensitive_name(rew->sensitiveType());
        // 脱敏后数据
        p->set_rewrite_data(rew->rewrite(value, encode_type_));
        // 字段名称
        p->set_field_name(identify->fieldName());
        // 脱敏策略名称
        p->set_rewrite_strategy_name(rules[item.id()]->rewriteStrategyName());
        new_length += item.start() - pos;
        new_length += new_data.back().length();
        pos = item.end();
      }
      rewrite_count++;
    }
  }
  // 时间
  log_.set_time(time_now);
  // 业务IP
  log_.set_downstream(getDownstreamAddress()->asString());
  // 脱敏条数
  log_.set_sanitized_data_count(rewrite_count);

  new_length += data_buf_in.length() - pos;
  auto reservation = data_buf_out.reserveSingleSlice(new_length);
  pos = 0;
  j = 0;
  const char* input = data_buf_in.data();
  char* output = static_cast<char*>(reservation.slice().mem_);
  for (const auto& item : records) {
    if (item.flag()) {
      auto iter = std::find(std::begin(id_), std::end(id_), item.id());
      if (iter != std::end(id_)) {
        const int len = item.start() - pos;
        ASSERT(len >= 0);

        memcpy(output, input + pos, len);
        output += len;
        const auto sz = new_data[j].length();
        memcpy(output, new_data[j].c_str(), sz);
        output += sz;

        pos = item.end();
        j++;
      }
    }
  }
  const int end_len = data_buf_in.size() - pos;
  ASSERT(end_len >= 0);
  if (end_len > 0) {
    memcpy(output, input + pos, end_len);
  }
  reservation.commit(new_length);
}

void ResponseRewrite::matchCollectRules(
    const ResponseRewriteRouteConfig* filter_vh_config,
    const ResponseRewriteRouteConfig* filter_route_config,
    const Network::Address::InstanceConstSharedPtr downstreamAddress,
    const Http::RequestHeaderMap& headers, const std::string& username) {

  auto matchRules = [&](const auto& rules, const ResponseRewriteRouteConfig* filter_config) {
    std::unordered_map<std::string, bool> sensitive_type;
    bool flag;
    auto whitelist = filter_config->whiteList();
    auto ip_whitelist = whitelist.ipWhiteListPtr();
    auto user_whitelist = whitelist.userWhiteListPtr();
    for (auto& ip : ip_whitelist) {
      if (ip->matchIp(downstreamAddress) && ip->enable()) {
        auto sensitive_ip = splitString(ip->sensitiveType());
        for (auto& result : sensitive_ip) {
          sensitive_type[result] = ip->sensitiveTypeAll();
        }
      }
    }

    for (auto& user : user_whitelist) {
      if (user->matchUsername(username) && user->enable()) {
        auto sensitive_name = splitString(user->sensitiveType());
        for (auto& result : sensitive_name) {
          sensitive_type[result] = user->sensitiveTypeAll();
        }
      }
    }

    for (size_t id = 0; id < rules.size(); id++) {
      flag = false;
      auto identify_rule = rules[id]->identifyRule();
      if (rules[id]->match(downstreamAddress, headers, username)) {
        for (const auto& pair : sensitive_type) {
          if (pair.first == identify_rule->sensitveTypeName() || pair.second == true) {
            flag = true;
            break;
          }
        }
        if (!flag) {
          id_.insert(id);
        }
      }
    }
  };

  if (filter_route_config != nullptr) {
    const auto& route_rules = filter_route_config->rules();
    if (!route_rules.empty()) {
      matchRules(route_rules, filter_route_config);
    }
  }

  if (filter_vh_config != nullptr) {
    const auto& vh_rules = filter_vh_config->rules();
    if (!vh_rules.empty()) {
      matchRules(vh_rules, filter_vh_config);
    }
  }
}

void ResponseRewrite::onStreamComplete() {
  if (log_.matched_patterns().size() > 0) {
    log(MessageUtil::getJsonStringFromMessageOrDie(log_, false, true));
  }
}

inline const ResponseRewriteRouteConfig* ResponseRewrite::getConfig() const {
  const auto* config =
      Http::Utility::resolveMostSpecificPerFilterConfig<ResponseRewriteRouteConfig>(
          filter_name_, encoder_callbacks_->route());

  return config;
}

inline const Envoy::Router::VirtualHostImpl* ResponseRewrite::getVirtualHost() const {
  auto route = decoder_callbacks_->route();
  if (route) {
    auto entry = route->routeEntry();
    if (entry) {
      auto& vh = entry->virtualHost();
      return dynamic_cast<const Envoy::Router::VirtualHostImpl*>(&vh);
    } else {
      auto route_impl = std::dynamic_pointer_cast<const Envoy::Router::RouteEntryImplBase>(route);
      if (route_impl && route_impl->isDirectResponse()) {
        const Envoy::Router::VirtualHost& vh = route_impl->virtualHost();
        return dynamic_cast<const Envoy::Router::VirtualHostImpl*>(&vh);
      }
    }
  }

  return nullptr;
}

inline const ResponseRewriteRouteConfig*
ResponseRewrite::getVirtualHostConfig(const Envoy::Router::VirtualHostImpl* vh) const {
  const ResponseRewriteRouteConfig* filter_vh_config = nullptr;
  if (vh != nullptr) {
    auto config = vh->perFilterConfig(filter_name_);
    if (config != nullptr) {
      filter_vh_config = dynamic_cast<const ResponseRewriteRouteConfig*>(config);
    }
  }

  return filter_vh_config;
}

inline const ResponseRewriteRouteConfig*
ResponseRewrite::getRouteConfig(const ResponseRewriteRouteConfig* filter_vh_config) const {
  const ResponseRewriteRouteConfig* filter_route_config = nullptr;
  auto route = decoder_callbacks_->route();
  if (route) {
    filter_route_config = dynamic_cast<const ResponseRewriteRouteConfig*>(
        route->mostSpecificPerFilterConfig(filter_name_));
  }

  // 路由级别的配置不存在时，mostSpecificPerFilterConfig返回的是VH级别的配置
  return filter_route_config == filter_vh_config ? nullptr : filter_route_config;
}

inline const Network::Address::InstanceConstSharedPtr
ResponseRewrite::getDownstreamAddress() const {
  return decoder_callbacks_->streamInfo().downstreamAddressProvider().remoteAddress();
}

inline const std::string& ResponseRewrite::getUpstreamName() const {
  static const std::string empty;
  Upstream::ClusterInfoConstSharedPtr cluster =
      decoder_callbacks_->streamInfo().upstreamClusterInfo().has_value()
          ? decoder_callbacks_->streamInfo().upstreamClusterInfo().value()
          : nullptr;

  return cluster ? cluster->name() : empty;
}

inline bool ResponseRewrite::getUserInfo(std::string& user_name) const {
  auto& metadata = decoder_callbacks_->streamInfo().dynamicMetadata().filter_metadata();
  if (!metadata.contains(SrhinoDomainMetaData)) {
    return false;
  }

  auto& filter_meta = metadata.at(SrhinoDomainMetaData);
  auto& fields = filter_meta.fields();
  if (fields.contains(SrhinoKeyUserName)) {
    user_name = fields.at(SrhinoKeyUserName).string_value();
    return true;
  }
  return false;
}

} // namespace ResponseRewrite
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
