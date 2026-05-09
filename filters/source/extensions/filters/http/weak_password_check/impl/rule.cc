#include "rule.h"
namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace WeakPasswordCheck {
namespace Impl {

/**
 * 用户名，密码名正则表达式
 */
const RegexBlock UserInfoRegex[ContentType::CONTENT_TYPE_MAX] = {
  {R"((^|[?&])KEYWORD=([^&]+))", 2},
  {R"("KEYWORD"[\s:]+"([^\n"]+)["])", 1},
  {R"(<KEYWORD>([^<^>]+)</KEYWORD>)", 1},
};

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

/**
 * https://blog.csdn.net/bulaganhuisi/article/details/126942290
 *  ignore resources map
 * 后缀名长度为2-4
 * */
static const std::unordered_set<std::string_view> IgnoreHttpResourceMap = {
  "avi",
  "azw",
  "bin",
  "bmp",
  "bz",
  "bz2",
  "csh",
  "css",
  "csv",
  "doc",
  "docx",
  "eot",
  "epub",
  "gif",
  "ico",
  "jar",
  "jpeg",
  "jpg",
  "js",
  "mid",
  "midi",
  "mjs",
  "mp3",
  "mpeg",
  "mpkg",
  "otf",
  "png",
  "pdf",
  "ppt",
  "pptx",
  "rar",
  "rtf",
  "sh",
  "svg",
  "swf",
  "tar",
  "tif",
  "tiff",
  "ttf",
  "txt",
  "vsd",
  "wav",
  "weba",
  "webm",
  "webp",
  "woff",
  "woff2",
  "xls",
  "xlsx",
  "zip",
  "3gp",
  "3g2",
  "7z",
};

bool WpdUtility::isIgnoreResourcesRequest(const std::string_view& path) {
  auto end = path.find_first_of('?');
  auto start = path.find_last_of('/', end);
  if (start == std::string_view::npos) {
    // std::cout << "not found" << std::endl;
    return false;
  }
  std::string_view absl_path(path.substr(start, end - start));
  // std::cout << absl_path << std::endl;
  auto pos = absl_path.find_last_of('.');
  if (pos == std::string_view::npos) {
    // std::cout << "not found" << std::endl;
    return false;
  } else {
    pos++;
    std::string_view postfix(absl_path.substr(pos));
    // std::cout << "postfix: " << postfix << std::endl;
    if (postfix.size() <= 1 || postfix.size() >= 5) {
      return false;
    }
    if (IgnoreHttpResourceMap.find(postfix) != IgnoreHttpResourceMap.end()) {
      ENVOY_LOG(debug, "ignore resource： {}", postfix);
      return true;
    }
  }

  return false;
}

PossibleEncryptType WpdUtility::getPossibleEncryptType(const std::string& password) {
  //TODO sha1 是40位
  if (password.size() % 16 != 0) {
    return POSSIBLE_ENCRYPT_TYPE_UNKNOWN;
  }
  for(auto it = password.cbegin(); it != password.cend(); it++) {
    if (!std::isxdigit(*it)) {
      return POSSIBLE_ENCRYPT_TYPE_UNKNOWN;
    }
  }
  switch(password.size()) {
    case 16:
    case 32:
      return POSSIBLE_ENCRYPT_TYPE_MD5_SHA256_SM3;

    case 20:
    case 64:
    default:
      return POSSIBLE_ENCRYPT_TYPE_UNKNOWN;
  }
}

bool WpdUtility::isNameReverse(const std::string& user_data, const std::string& password_data) {
  auto str = user_data;
  std::reverse(str.begin(), str.end());
  if (password_data.size() == 16) {
    // ENVOY_LOG(debug, "md5: {}, 16: {}", MD5(user_data), MD5(user_data).substr(8, 16));
    if (MD5(user_data).substr(8, 16) == password_data || MD5(str).substr(8, 16) == password_data) {
      return true;
    }
  } else if (password_data.size() == 32) {
    // ENVOY_LOG(debug, "md5: {}", MD5(user_data));
    if (MD5(user_data) == password_data || MD5(str) == password_data) {
      return true;
    }
    ENVOY_LOG(debug, "sha1: {}", SHA1(user_data));
    if (SHA1(user_data) == password_data || SHA1(str) == password_data) {
      return true;
    }
  } else if (password_data.size() == 64) {
    ENVOY_LOG(debug, "sha256: {}", SHA256(user_data));
    if (SHA256(user_data) == password_data || SHA256(str) == password_data) {
      return true;
    }
  }
  return false;
}

bool WpdUtility::isBase64(const std::string& data) {
  if (data.size() == 0 || data.size() % 4 != 0) {
    return false;
  }

  for (char c : data) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
        || c == '+' || c == '/' || c == '=') {
      continue;
    } else {
      return false;
    }
  }

  return true;
}

bool WpdUtility::tryDecodeBase64(const std::string& data, std::string& decoded_data) {
  decoded_data = Envoy::Base64::decode(data);
  for (char c : decoded_data) {
    if (!std::isprint(c)) {
      return false;
    }
  }
  return true;
}

bool WpdUtility::isPasswordWeak(const std::string& user_name, const std::string& password) {
  // 密码长度太短
  if (password.size() <= WeakPasswordLength) {
    ENVOY_LOG(debug, "too short: {}", password.size());
    return true;
  }

  // 只使用一种字符类型，如字母、数字、或特殊字符的一种
  // 连续的字符，如 123456, abcd, qwert
  // 包含大量重复, 如 1111， aaaaa
  struct Stats {
    uint16_t digits; // 数字个数
    uint16_t alphas; // 字母个数
    uint16_t others; // 其它个数
    uint16_t groups; // 字符去重个数

    uint16_t max_repeats; // 最大的重复字符数
    char     max_repeat_ch;

    uint16_t max_continues; // 最大的连续字符数
    char     max_continue_ch;
  } stat;
  memset(&stat, 0, sizeof(stat));
  uint16_t map[256] = {0};
  char prev = password.at(0);
  uint16_t repeat_cnt = 0;
  uint16_t continue_cnt = 1;
  bool is_add = true;
  for (const auto ch : password) {
    map[static_cast<int>(ch)]++;
    // 重复判断
    if (ch == prev) {
      repeat_cnt++;
    } else {
      if (repeat_cnt > stat.max_repeats) {
        stat.max_repeats = repeat_cnt;
        stat.max_repeat_ch = ch;
      }
      repeat_cnt = 1;
    }
    // 连续判断 TODO qwert
    if (ch == (prev + 1)) {
      if (is_add) {
        continue_cnt++;
      } else {
        is_add = true;
        if (continue_cnt > stat.max_continues) {
          stat.max_continues = continue_cnt;
          stat.max_continue_ch = ch;
        }
        continue_cnt = 2;
      }
    } else if (ch == (prev - 1)) {
      if (is_add) {
        is_add = false;
        if (continue_cnt > stat.max_continues) {
          stat.max_continues = continue_cnt;
          stat.max_continue_ch = ch;
        }
        continue_cnt = 2;
      } else {
        continue_cnt++;
      }
    } else {
      if (continue_cnt > stat.max_continues) {
        stat.max_continues = continue_cnt;
        stat.max_continue_ch = ch;
      }
      continue_cnt = 1;
    }
    prev = ch;

    // 字符类型统计
    if (std::isdigit(ch)) {
      stat.digits++;
    } else if (std::isalpha(ch)) {
      stat.alphas++;
    } else {
      stat.others++;
    }
  }
  // 重复判断
  if (repeat_cnt > stat.max_repeats) {
    stat.max_repeats = repeat_cnt;
    stat.max_repeat_ch = prev;
  }
  if (continue_cnt > stat.max_continues) {
    stat.max_continues = continue_cnt;
    stat.max_continue_ch = prev;
  }

  // std::cout << "user_name: " << user_name << "   password: " << password << std::endl;
  // std::cout << "digits: " << stat.digits << " alphas: " << stat.alphas << " others: " << stat.others << std::endl;
  // std::cout << "max_repeats: " << stat.max_repeats << " max_repeat_ch: " << stat.max_repeat_ch << std::endl;
  // std::cout << "max_continues: " << stat.max_continues << " max_continue_ch: " << stat.max_continue_ch << std::endl;
  int type = 0;
  if (stat.digits) type++;
  if (stat.alphas) type++;
  if (stat.others) type++;
  // 字符种类单一
  if (type <= 1) {
    ENVOY_LOG(debug, "type too small: {}", type);
    return true;
  }
  // 重复太长
  if (stat.max_repeats >= 3 && (password.size() - stat.max_repeats) <= (WeakPasswordLength - 1)) {
    ENVOY_LOG(debug, "repeat too long: {}", stat.max_repeats);
    return true;
  }
  // 连续太长
  if (stat.max_continues >= 3 && (password.size() - stat.max_continues) <= (WeakPasswordLength - 1)) {
    ENVOY_LOG(debug, "continue too long: {}", stat.max_continues);
    return true;
  }

  for (int i = 0; i < 256; i++) {
    if (map[i]) {
      stat.groups++;
      //std::cout << (char)i << ": " << map[i] << std::endl;
    }
  }
  // 字符单一，如ab4ab4ab4ab4
  if (stat.groups <= 3) {
    ENVOY_LOG(debug, "group too small: {}", stat.groups);
    return true;
  }

  // 密码从用户名中取字符
  // for (const auto ch : password) {
  //   if (user_name.find(ch) == std::string::npos) {
  //     return false;
  //   }
  // }

  if (user_name.compare(password) == 0) {
    ENVOY_LOG(debug, "the password and user_name are the same");
    return true;
  }
  return false;
}

/**
 * hex_string 默认为小写十六进制字符串
*/
uint64_t WpdUtility::hexStringToUint64(const std::string& hex_string) {
  uint64_t result = 0;
  std::for_each(hex_string.cbegin(), hex_string.cend(), [&](const uint8_t c) {
    result <<= 4;
    if (std::isdigit(c)) {
      result += c - '0';
    } else {
      result += c - 'a' + 10;
    }
  });
  return result;
}

uint64_t WpdUtility::getKeyByEncryptType(const std::string& encrypt_password, enum EncryptType type) {
  enum PossibleEncryptType possible_type;
  switch (type) {
  case ENCRYPT_TYPE_MD5:
  case ENCRYPT_TYPE_SHA256:
  case ENCRYPT_TYPE_SM3:
    possible_type = POSSIBLE_ENCRYPT_TYPE_MD5_SHA256_SM3;
    break;
  case ENCRYPT_TYPE_SHA1:
    possible_type = POSSIBLE_ENCRYPT_TYPE_SHA1;
    break;
  case ENCRYPT_TYPE_SHA512:
  case ENCRYPT_TYPE_SHA3:
    possible_type = POSSIBLE_ENCRYPT_TYPE_SHA512_SHA3;
    break;
  default:
    ENVOY_LOG(error, "unsupport type: {}", type);
    return 0;
  }

  return getKeyByPossibleEncryptType(encrypt_password, possible_type);
}

uint64_t WpdUtility::getKeyByPossibleEncryptType(const std::string& encrypt_password, enum PossibleEncryptType type) {
  switch(type) {
    case POSSIBLE_ENCRYPT_TYPE_MD5_SHA256_SM3:
      return hexStringToUint64(encrypt_password.substr(8, 16));
    case POSSIBLE_ENCRYPT_TYPE_SHA1:
      return hexStringToUint64(encrypt_password.substr(2, 16));
    case POSSIBLE_ENCRYPT_TYPE_SHA512_SHA3:
      return hexStringToUint64(encrypt_password.substr(24, 16));
    default:
      ENVOY_LOG(error, "unsupport type: {}", type);
      break;
  }
  return 0;
}

// user_name为用户名数据
std::string WpdUtility::MD5(const std::string& user_name) {
  // 创建一个MD5对象
  CryptoPP::Weak1::MD5 md5;
  // 计算MD5哈希值
  CryptoPP::byte digest[CryptoPP::Weak1::MD5::DIGESTSIZE];
  md5.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(user_name.c_str()),
                      user_name.length());
  // 将哈希值转换为十六进制字符串
  std::stringstream ss;
  for (int i = 0; i < CryptoPP::Weak1::MD5::DIGESTSIZE; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
  }
  std::string hash = ss.str();
  return hash;
}

std::string& WpdUtility::deSensitive(std::string& password) {
  if (password.size() <= 6) {
    std::transform(password.begin(), password.end(), password.begin(), [](uint8_t) { return '*';});
  } else {
    std::transform(password.begin() + 1, password.end() - 1, password.begin() + 1, [](uint8_t) { return '*';});
  }
  return password;
}

std::string WpdUtility::SHA256(const std::string& user_name) {
  CryptoPP::SHA256 sha1;
  CryptoPP::byte digest[CryptoPP::SHA256::DIGESTSIZE];
  sha1.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(user_name.c_str()),
                       user_name.length());
  std::stringstream ss;
  for (int i = 0; i < CryptoPP::SHA256::DIGESTSIZE; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
  }
  std::string hash = ss.str();
  return hash;
}

std::string WpdUtility::SHA1(const std::string& user_name) {
  CryptoPP::SHA1 sha1;
  // CryptoPP::byte
  CryptoPP::byte digest[CryptoPP::SHA1::DIGESTSIZE];
  sha1.CalculateDigest(digest, reinterpret_cast<const CryptoPP::byte*>(user_name.c_str()),
                       user_name.length());
  std::stringstream ss;
  for (int i = 0; i < CryptoPP::SHA1::DIGESTSIZE; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
  }
  std::string hash = ss.str();

  return hash;
}

#ifdef __aarch64__
const std::string AutoConfig::user_info_matchers_db_path_{"plugins/weak_password_check/conf/aarch64/user_info"};
const std::string AutoConfig::token_matchers_db_path_{"plugins/weak_password_check/conf/aarch64/token"};
#else
const std::string AutoConfig::user_info_matchers_db_path_{"plugins/weak_password_check/conf/x86_64/user_info"};
const std::string AutoConfig::token_matchers_db_path_{"plugins/weak_password_check/conf/x86_64/token"};
#endif

RegexMatcherPtr
AutoConfig::regexMatcherInit(const std::vector<RegexMatcher::ExpressionView>& exprs, const std::string& db_path) {
  // init regex matcher
  RegexMatcherPtr matcher = std::make_shared<RegexMatcher::RegexMatcher>(
      db_path, RegexMatcher::RegexMode::RegexModeBlock, RegexMatcher::RegexType::RegexHyperscan, true);
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

RegexMatcherPtr AutoConfig::regexMatcherInit(const RegexGroup& regex_group, const std::string& db_path) {
  std::vector<RegexMatcher::ExpressionView> exprs;
  for (size_t i = 0; i < regex_group.size(); i++) {
    exprs.emplace_back(RegexMatcher::ExpressionView(regex_group[i].regex_str, i, true));
    // ENVOY_LOG(trace, "add pattern: {}, id: {}", regex_group[i].regex_str, i);
  }
  return regexMatcherInit(exprs, db_path);
}

MatcherBlockPtr AutoConfig::initMatcherBlock(const std::string& matcher_name,
                                             const std::vector<std::string>& user_names,
                                             const std::vector<std::string>& password_names,
                                             int group_size, const std::string& db_path) {
  MatcherBlockPtr matcher_block = std::make_shared<MatcherBlock>();
  matcher_block->regex_groups_.resize(group_size);
  for (auto i = 0; i < group_size; i++) {
    for (auto& it : user_names) {
      RegexBlock block;
      block.regex_str = stringReplace(UserInfoRegex[i].regex_str, "KEYWORD", it);
      block.extract_group = UserInfoRegex[i].extract_group;
      block.re = std::regex(block.regex_str, std::regex_constants::icase);
      block.type = FIELD_TYPE_USER_NAME;
      matcher_block->regex_groups_[i].emplace_back(block);
    }
    for (auto& it : password_names) {
      RegexBlock block;
      block.regex_str = stringReplace(UserInfoRegex[i].regex_str, "KEYWORD", it);
      block.extract_group = UserInfoRegex[i].extract_group;
      block.re = std::regex(block.regex_str, std::regex_constants::icase);
      block.type = FIELD_TYPE_PASSWORD_NAME;
      matcher_block->regex_groups_[i].emplace_back(block);
    }
  }
  matcher_block->regex_matchers_.resize(group_size);
  for (auto i = 0; i < group_size; i++) {
    matcher_block->regex_matchers_[i] =
        AutoConfig::regexMatcherInit(matcher_block->regex_groups_[i], db_path.empty() ? db_path : (db_path + "_group" + std::to_string(i)));
    if (matcher_block->regex_matchers_[i]) {
      ENVOY_LOG(trace, "{} group: {} init success", matcher_name, i);
    } else {
      ENVOY_LOG(trace, "{} group: {} init failed", matcher_name, i);
    }
  }

  ENVOY_LOG(trace, "{} matcher_block init success", matcher_name);
  return matcher_block;
}

MatcherBlockPtr AutoConfig::initTokenMatcherBlock(const std::string& matcher_name,
                                                  const std::vector<std::string>& token_names,
                                                  int group_size, const std::string& db_path) {
  MatcherBlockPtr matcher_block = std::make_shared<MatcherBlock>();
  matcher_block->regex_groups_.resize(group_size);
  for (auto i = 0; i < group_size; i++) {
    for (auto& it : token_names) {
      RegexBlock block;
      block.regex_str = stringReplace(TokenRegex[i].regex_str, "KEYWORD", it);
      block.extract_group = TokenRegex[i].extract_group;
      block.re = std::regex(block.regex_str, std::regex_constants::icase);
      // block.type = FIELD_TYPE_USER_NAME;
      matcher_block->regex_groups_[i].emplace_back(block);
    }
  }
  matcher_block->regex_matchers_.resize(group_size);
  for (auto i = 0; i < group_size; i++) {
    matcher_block->regex_matchers_[i] =
        AutoConfig::regexMatcherInit(matcher_block->regex_groups_[i], db_path.empty() ? db_path : (db_path + "_group" + std::to_string(i)));
    if (matcher_block->regex_matchers_[i]) {
      ENVOY_LOG(trace, "{} group: {} init success", matcher_name, i);
    } else {
      ENVOY_LOG(trace, "{} group: {} init failed", matcher_name, i);
    }
  }

  ENVOY_LOG(trace, "{} matcher_block init success", matcher_name);
  return matcher_block;
}

/**
 * rainbow_orig.db 中的 weak_md5表是原始的弱密码表
 * rainbow.db 中的 weak_password表是要生成的新的弱密码表
*/
void AutoConfig::migrate() {
  const std::string DbPathRainbowOrig{"plugins/weak_password_check/conf/rainbow_orig.db"};
  RainbowDbPtr old_db(std::make_shared<RainbowDb>(DbPathRainbowOrig));
  RainbowDbPtr new_db(std::make_shared<RainbowDb>());
  if (!new_db->createWeakPasswordTable()) {
    ENVOY_LOG(error, "create table failed");
    return;
  }

  std::vector<struct WeakMd5Item> old_items;
  old_db->queryWeakMd5Table(old_items);
  ENVOY_LOG(error, "old weak_md5 size: {}", old_items.size());
  //check
  // for (const auto& item : old_items) {
  //   if (WpdUtility::MD5(item.plain_text_password) != item.md5) {
  //     ENVOY_LOG(error, "password: {} not match md5: {}, correct md5 is: {}", item.plain_text_password, item.md5, WpdUtility::MD5(item.plain_text_password));
  //   }
  // }
  // ENVOY_LOG(error, "check old weak_md5 done");

  //transform
  // md5
  std::vector<struct WeakPasswordItem> new_items;
  std::transform(old_items.cbegin(), old_items.cend(), std::inserter(new_items, new_items.begin()),
            [&](const struct WeakMd5Item& in) {
              struct WeakPasswordItem new_item;
              GENERATE_MD5_WEAK_PASSWORD_ITEM(new_item, in.plain_text_password);
              // ENVOY_LOG(error, "encrypt_password: {} key: {}", new_item.encrypt_password, new_item.key);
              return new_item;
            });
  // test duplicate insert
  // int pos = 10;
  // std::transform(old_items.cbegin() + pos, old_items.cbegin() + pos + 1, std::inserter(new_items, new_items.begin() + pos + 200),
  //           [&](const struct PasswordItem& in) {
  //             struct WeakPasswordItem new_item;
  //             new_item.plain_password = in.plain_text_password,
  //             new_item.encrypt_type = ENCRYPT_TYPE_MD5,
  //             new_item.encrypt_password = WpdUtility::MD5(new_item.plain_password),
  //             new_item.key = WpdUtility::getKeyByEncryptType(new_item.encrypt_password, ENCRYPT_TYPE_MD5);
  //             // ENVOY_LOG(error, "encrypt_password: {} key: {}", new_item.encrypt_password, new_item.key);
  //             return new_item;
  //           });
  uint32_t count = new_db->insertWeakPasswordTable(new_items);
  ENVOY_LOG(error, "new items size:{}, insert: {}", new_items.size(), count);

  // sha1
  ENVOY_LOG(error, "create new weak_password done");
  return;

  //insert

}

AutoConfig::AutoConfig(const v3::WeakPasswordCheckGlobal& proto_config)
    : rainbow_(std::make_shared<RainbowDb>()) {
  if (proto_config.has_custom_password()) {
    custom_password_ = std::make_shared<CustomPassword>(proto_config.custom_password());
  }

  // migrate(); return;

  std::vector<std::string> user_names;
  if (!rainbow_->getUserNames(user_names)) {
    ENVOY_LOG(error, "query username from failed!");
    return;
  }
  std::vector<std::string> password_names;
  if (!rainbow_->getPasswordNames(password_names)) {
    ENVOY_LOG(error, "query password names from failed!");
    return;
  }
  user_info_matchers_ =
      initMatcherBlock("user_info_matchers", user_names, password_names, CONTENT_TYPE_MAX, AutoConfig::user_info_matchers_db_path_);

  // init token matcher
  std::vector<std::string> token_names;
  if (!rainbow_->getTokenNames(token_names)) {
    ENVOY_LOG(error, "query token names from failed!");
    return;
  }
  token_matchers_ = initTokenMatcherBlock("token_matchers", token_names, TOKEN_TYPE_MAX, AutoConfig::token_matchers_db_path_);
}

AutoConfig::~AutoConfig() {
  ENVOY_LOG(debug, __FUNCTION__);
}

/**
 * 将匹配结果分类
 * 返回值:
 *  true 用户名个数与密码名个数是否相等
*/
bool AutoConfig::splitUserInfoMatchResults(
    const std::vector<RegexMatcher::MatchResult>& results, const RegexGroup& regex_group,
    std::vector<RegexMatcher::MatchResult>& user_name_results,
    std::vector<RegexMatcher::MatchResult>& password_name_results) {
  for (auto& it : results) {
    ENVOY_LOG(trace, "id: {}, start: {}, end: {}", it.id(), it.start(), it.end());
    uint32_t id = it.id();
    if (id >= regex_group.size()) {
      return false;
    }
    if (regex_group[id].type == FIELD_TYPE_USER_NAME) {
      user_name_results.push_back(it);
    } else {
      password_name_results.push_back(it);
    }
  }
  return user_name_results.size() && password_name_results.size();
}

bool AutoConfig::realExtract(const std::string_view& data_view, const RegexGroup& group,
                             const RegexMatcher::MatchResult& result, std::string& str) {
  std::string_view match_piece(data_view.data() + result.start(), result.end() - result.start());
  ENVOY_LOG(trace, "id: {}, start: {}, end: {}", result.id(), result.start(),
            result.end());
  const RegexBlock& block = group[result.id()];
  return regex_extract(match_piece, block.re, block.extract_group, str);
}

bool AutoConfig::realExtractUserInfo(RegexMatcherPtr regex_matcher, RegexGroup& regex_group,
                                     const std::string_view& data_view, int thread_index,
                                     std::vector<std::pair<std::string, std::string>>& user_info) {
  // 扫描
  std::vector<RegexMatcher::MatchResult> results;
  if (0 != regex_matcher->match(data_view, results, RegexMatcher::EncodingType::UTF8, thread_index)) {
    return false;
  }
  if (results.empty()) {
    return false;
  }

  std::vector<RegexMatcher::MatchResult> user_name_results;
  std::vector<RegexMatcher::MatchResult> password_name_results;
  if (!splitUserInfoMatchResults(results, regex_group, user_name_results, password_name_results)) {
    ENVOY_LOG(debug, "extract user info failed");
    return false;
  }

  for (size_t i = 0; i < user_name_results.size() && i < password_name_results.size(); i++) {
    std::string user_name;
    std::string password;
    realExtract(data_view, regex_group, user_name_results[i], user_name);
    realExtract(data_view, regex_group, password_name_results[i], password);
    user_info.emplace_back(std::make_pair(std::move(user_name), std::move(password)));
  }
  return true;
}

bool AutoConfig::doExtractUserInfo(const std::string_view& data_view, enum ContentType type,
                                 int thread_index,
                                 std::vector<std::pair<std::string, std::string>>& user_info) {
  if (type >= CONTENT_TYPE_MAX || !user_info_matchers_) {
    return false;
  }
  RegexMatcherPtr regex_matcher = user_info_matchers_->regex_matchers_[type];
  if (!regex_matcher) {
    return false;
  }
  RegexGroup &regex_group = user_info_matchers_->regex_groups_[type];
  return realExtractUserInfo(regex_matcher, regex_group, data_view, thread_index, user_info);
}

bool AutoConfig::extractUserInfo(const std::string&, const std::string_view& data_view,
                                 enum ContentType type, int thread_index,
                                 std::vector<std::pair<std::string, std::string>>& user_info) {
  return doExtractUserInfo(data_view, type, thread_index, user_info);
}


bool AutoConfig::isLoginSuccess(const std::string_view& data_view, enum TokenRegexType type, int thread_index) {
  if (type >= TOKEN_TYPE_MAX || !token_matchers_) {
    return false;
  }
  RegexMatcherPtr regex_matcher = token_matchers_->regex_matchers_[type];
  if (!regex_matcher) {
    return false;
  }

  std::vector<RegexMatcher::MatchResult> results;
  if (0 != regex_matcher->match(data_view, results, RegexMatcher::EncodingType::UTF8, thread_index)) {
    return false;
  }
  return results.empty() ? false : true;
}

bool AutoConfig::queryPlainPassword(const std::string& password, enum PossibleEncryptType type, std::string& plain_password, enum TrigRuleType& trig_type) {
  uint64_t key = WpdUtility::getKeyByPossibleEncryptType(password, type);
  //TODO 根据type定位不同的表
  if (rainbow_->getPlainPassword(key, plain_password)) {
    ENVOY_LOG(debug, "find weak password in rainbow: {}", plain_password);
    trig_type = TrigInternal;
    return true;
  }

  if (custom_password_ && custom_password_->getPlainPassword(key, plain_password)) {
    ENVOY_LOG(debug, "find weak password in custom rainbow: {}", plain_password);
    trig_type = TrigCustom;
    return true;
  }

  return false;
}

CustomRule::CustomRule(const v3::AdvanceConfig& advance_config)
    : cluster_name_(advance_config.cluster_name().cbegin(), advance_config.cluster_name().cend()) {
  if (advance_config.user_name_size() > 0) {
    std::vector<std::string> user_names(advance_config.user_name().cbegin(),
                                        advance_config.user_name().cend());
    std::vector<std::string> password_names(advance_config.password_name().cbegin(),
                                            advance_config.password_name().cend());
    user_info_matchers_ = AutoConfig::initMatcherBlock("custom_user_info_matchers", user_names,
                                                       password_names, CONTENT_TYPE_MAX);
  }
}

AdvancedConfig::AdvancedConfig(const v3::WeakPasswordCheckGlobal& proto_config) :
  AutoConfig(proto_config) {

  for (const auto& it : proto_config.advance_config()) {
    rules_.emplace_back(std::make_shared<CustomRule>(it));
  }
  for (const auto& it : rules_) {
    ENVOY_LOG(trace, "cluster_name size:{}", it->cluster_name().size());
    for (const auto& cluster_name : it->cluster_name()) {
      if (cluster_map_.end() != cluster_map_.find(cluster_name)) {
        ENVOY_LOG(warn, "cluster name duplicate: {}, ignore", cluster_name);
        continue;
      }
      cluster_map_[cluster_name] = it;
    }
  }

}

bool AdvancedConfig::doExtractUserInfo(
    const CustomRulePtr rule, const std::string_view& data_view, enum ContentType type,
    int thread_index, std::vector<std::pair<std::string, std::string>>& user_info) {
  const MatcherBlockPtr matcher_block = rule->user_info_matchers();
  if (!matcher_block) {
    ENVOY_LOG(debug, "auto_mode");
    return false;
  }
  RegexMatcherPtr regex_matcher = matcher_block->regex_matchers_[type];
  if (!regex_matcher) {
    return false;
  }
  RegexGroup& regex_group = matcher_block->regex_groups_[type];
  return realExtractUserInfo(regex_matcher, regex_group, data_view, thread_index, user_info);
}

bool AdvancedConfig::extractUserInfo(const std::string& cluster_name,
                                     const std::string_view& data_view, enum ContentType type,
                                     int thread_index,
                                     std::vector<std::pair<std::string, std::string>>& user_info) {
  if (cluster_name.empty() || (type >= CONTENT_TYPE_MAX)) {
    return false;
  }

  auto rule = cluster_map_.find(cluster_name);
  if (rule == cluster_map_.end()) {
    return false;
  }
  if (doExtractUserInfo(rule->second, data_view, type, thread_index, user_info)) {
    return true;
  }
  // 如果高级配置无法获取到密码，则使用内置规则获取
  return AutoConfig::doExtractUserInfo(data_view, type, thread_index, user_info);
}

} // namespace Impl
} // namespace WeakPasswordCheck
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
