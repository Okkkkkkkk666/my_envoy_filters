#include "contrib/common/fpe/fpe.h"

#include <openssl/evp.h>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <shared_mutex>
#include <vector>
#include <fstream>
#include <mutex>
namespace Envoy {
namespace Extensions {
namespace Common {
namespace FPE {

namespace {

// 数字字符集 (0-9)
const std::string ALPHA_DIGIT = "0123456789";

//小写字母字符 (a-z)
const std::string ALPHA_LOWER = "abcdefghijklmnopqrstuvwxyz";

// 大写字母字符 (A-Z)
const std::string ALPHA_UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// 常见可打印符号字符集
const std::string ALPHA_SYMBOL = " !\"#$%&()*+,-./:;<=>?@[]^_`{|}~";

/**
 * @brief 字符分类枚举，用于区分数字、大小写、符号等类型
 */
enum class CharClass {
  DIGIT,    // 数字 0-9
  LOWER,    // 小写字母 a-z
  UPPER,    // 大写字母 A-Z
  SYMBOL,   // 可打印符号
  UNKNOWN,  // 未知字符（如中文、控制符）
};

/**
 * @brief 将字符转换为对应字符集中的索引值
 * @param c 输入字符
 * @param alphabet 字符集
 * @return 索引值，未找到返回 -1
 */
inline int charToInt(char c, const std::string& alphabet) {
  size_t pos = alphabet.find(c);
  if (pos != std::string::npos) {
    return static_cast<int>(pos);
  }
  return -1;
}

/**
 * @brief 将字符串按指定基数转换为 uint64_t
 * @param s 输入字符串
 * @param alphabet 字符集
 * @return 转换后的数值
 */
uint64_t stringToUint64(const std::string& s, const std::string& alphabet) {
  uint64_t res = 0;
  uint64_t radix = alphabet.length();
  for (char c : s) {
    res = res * radix + charToInt(c, alphabet);
  }
  return res;
}

/**
 * @brief 将 uint64_t 数值转回指定长度的字符串
 * @param n 输入数值
 * @param width 输出字符串长度
 * @param alphabet 字符集
 * @return 定长字符串
 */
std::string uint64ToString(uint64_t n, int width, const std::string& alphabet) {
  uint64_t radix = alphabet.length();
  std::string res = "";
  
  if (n == 0) {
    res = alphabet[0];
  }
  
  while (n > 0) {
    res += alphabet[static_cast<int>(n % radix)];
    n /= radix;
  }
  
  while (res.length() < static_cast<size_t>(width)) {
    res += alphabet[0];
  }
  
  std::reverse(res.begin(), res.end());
  return res;
}

/**
 * @brief 快速计算 base^exp
 * @param base 基数
 * @param exp 指数
 * @return 计算结果
 */
uint64_t pow64(uint64_t base, int exp) {
  uint64_t res = 1;
  while (exp > 0) {
    if (exp % 2 == 1) {
      res *= base;
    }
    base *= base;
    exp /= 2;
  }
  return res;
}

/**
 * @brief AES 加密引擎，基于 OpenSSL EVP 接口，ECB 模式，无填充
 */
class AESEngine {
public:
  /**
   * @brief 构造函数，初始化 AES-128 加密上下文
   * @param key 16 字节密钥
   */
  explicit AESEngine(const std::string& key) {
    ctx_ = EVP_CIPHER_CTX_new();
    uint8_t key128[16] = {0};
    std::memcpy(key128, key.data(), std::min(key.size(), static_cast<size_t>(16)));
    EVP_EncryptInit_ex(ctx_, EVP_aes_128_ecb(), nullptr, key128, nullptr);
    EVP_CIPHER_CTX_set_padding(ctx_, 0);
  }

  /**
   * @brief 析构函数，释放上下文
   */
  ~AESEngine() {
    if (ctx_ != nullptr) {
      EVP_CIPHER_CTX_free(ctx_);
    }
  }

  /**
   * @brief 对 16 字节数据进行 AES 加密
   * @param in 输入 16 字节
   * @param out 输出 16 字节
   */
  void encrypt(const uint8_t in[16], uint8_t out[16]) {
    int len;
    EVP_EncryptUpdate(ctx_, out, &len, in, 16);
  }

private:
  EVP_CIPHER_CTX* ctx_; /**< OpenSSL EVP 上下文 */
};

} // namespace

/**
 * @brief FPE 算法实现类，提供 FF1 格式保留加密
 */
class FPEImpl : public FPEInterface {
public:
  /**
   * @brief 直接通过密钥和 Tweak 初始化
   * @param key AES 密钥
   * @param tweak 微调值
   * @return 初始化成功返回 true
   */
  bool initFromKey(const std::string& key, const std::string& tweak) override {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    key_ = key;
    tweak_ = tweak;
    ready_ = true;
    return true;
  }

  /**
   * @brief 从配置文件加载 key 和 tweak
   * @param path 配置文件路径
   * @return 成功返回 true
   */
  bool initFromConfigFile(const std::string& path) override {
    std::ifstream in(path);
    if (!in) {
      return false;
    }

    std::string line;
    std::string k;
    std::string t;

    while (std::getline(in, line)) {
      if (line.find("key:") == 0) {
        k = line.substr(4);
      }
      if (line.find("tweak:") == 0) {
        t = line.substr(6);
      }
    }
    return initFromKey(k, t);
  }

  /**
   * @brief 加密入口
   * @param input 明文
   * @param type 数据类型（身份证、手机、邮箱等）
   * @return 密文
   */
  std::string encrypt(const std::string& input, const std::string& context, DataType type) override {
    return routeAndProcess(input, context, type, true);
  }

  /**
   * @brief 解密入口
   * @param input 密文
   * @param type 数据类型
   * @return 明文
   */
  std::string decrypt(const std::string& input, const std::string& context, DataType type) override {
    return routeAndProcess(input, context, type, false);
  }

private:
  std::string combineTweakAndContext(const std::string& base_tweak, const std::string& context) const {
  if (context.empty()) {
    return base_tweak;
  }
  if (base_tweak.empty()) {
    return context;
  }
  return base_tweak + "|" + context;
}
  /**
   * @brief 统一处理入口
   * @param input 输入串
   * @param type 数据类型
   * @param is_encrypt true=加密，false=解密
   * @return 处理结果
   */
  std::string routeAndProcess(const std::string& input, const std::string& context, DataType type, bool is_encrypt) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!ready_ || input.empty()) {
      return input;
    }
    
    std::string local_key = key_;
    std::string context_tweak = combineTweakAndContext(tweak_, context);
    
    if(type == DataType::RAW_TEXT){
      return processStrictFormat(input, is_encrypt, local_key, context_tweak);
    }else if(type == DataType::EMAIL){
      return processEmail(input, is_encrypt, local_key, context_tweak);
    }else if(type == DataType::ID_CARD){
      return processIdCard(input, is_encrypt, local_key, context_tweak);
    }else if(type == DataType::PHONE){
      return processPhone(input, is_encrypt, local_key, context_tweak);
    }else{
      return processStrictFormat(input, is_encrypt, local_key, context_tweak);
    }
  }

  /**
   * @brief 邮箱格式处理：只加密 @ 前的部分
   * @param input 邮箱字符串
   * @param is_encrypt 加密/解密
   * @param k 密钥
   * @param t tweak
   * @return 处理后邮箱
   */
  std::string processEmail(const std::string& input, bool is_encrypt, const std::string& k, const std::string& t) {
    size_t at_pos = input.find('@');
    if (at_pos == std::string::npos || at_pos < 2) {
      return processStrictFormat(input, is_encrypt, k, t); 
    }

    std::string local_part = input.substr(0, at_pos);
    std::string domain_part = input.substr(at_pos);
    
    std::string enc_local = processStrictFormat(local_part, is_encrypt, k, t);
    return enc_local + domain_part;
  }

  /**
   * @brief 身份证处理：前17位数字加密，最后一位校验位不变
   * @param input 身份证号
   * @param is_encrypt 加密/解密
   * @param k 密钥
   * @param t tweak
   * @return 处理后身份证号
   */
  std::string processIdCard(const std::string& input, bool is_encrypt, const std::string& k, const std::string& t) {
    if (input.length() < 18) {
      return processStrictFormat(input, is_encrypt, k, t);
    }

    std::string prefix_17 = input.substr(0, 17);
    char last_char = input.back();
    
    std::string enc_17 = coreFF1(prefix_17, is_encrypt, ALPHA_DIGIT, k, t); 
    return enc_17 + last_char;
  }

  /**
   * @brief 手机号处理：前3位不变，后8位加密
   * @param input 手机号
   * @param is_encrypt 加密/解密
   * @param k 密钥
   * @param t tweak
   * @return 处理后手机号
   */
  std::string processPhone(const std::string& input, bool is_encrypt, const std::string& k, const std::string& t) {
    std::string digits = "";
    for (char c : input) {
      if (std::isdigit(c)) {
        digits += c;
      }
    }
    
    if (digits.length() != 11) {
      return processStrictFormat(input, is_encrypt, k, t); 
    }
    
    std::string prefix = digits.substr(0, 3);
    std::string suffix = digits.substr(3); 
    std::string enc_suffix = coreFF1(suffix, is_encrypt, ALPHA_DIGIT, k, t);
    
    std::string result = "";
    int p_idx = 0;
    int s_idx = 0;

    for (char c : input) {
      if (!std::isdigit(c)) {
        result += c;
        continue;
      }
      if (p_idx < 3) {
        result += digits[p_idx++];
      } else {
        result += enc_suffix[s_idx++];
      }
    }
    return result;
  }

  /**
   * @brief 严格格式加密：数字、小写、大写、符号独立加密，保持原格式
   * @param input 输入字符串
   * @param is_encrypt 加密/解密
   * @param k 密钥
   * @param t tweak
   * @return 格式保留的加密/解密结果
   */
  std::string processStrictFormat(const std::string& input, bool is_encrypt, const std::string& k, const std::string& t) {
    std::string digits = "", lowers = "", uppers = "", symbols = "";
    std::vector<CharClass> layout(input.length());

    for (size_t i = 0; i < input.length(); ++i) {
      char c = input[i];
      if (std::isdigit(c)) {
        layout[i] = CharClass::DIGIT;
        digits += c;
      } else if (std::islower(c)) {
        layout[i] = CharClass::LOWER;
        lowers += c;
      } else if (std::isupper(c)) {
        layout[i] = CharClass::UPPER;
        uppers += c;
      } else if (ALPHA_SYMBOL.find(c) != std::string::npos) {
        layout[i] = CharClass::SYMBOL;
        symbols += c;
      } else {
        layout[i] = CharClass::UNKNOWN;
      }
    }

    std::string enc_digits = digits;
    if (digits.length() >= 2) {
      enc_digits = coreFF1(digits, is_encrypt, ALPHA_DIGIT, k, t);
    }

    std::string enc_lowers = lowers;
    if (lowers.length() >= 2) {
      enc_lowers = coreFF1(lowers, is_encrypt, ALPHA_LOWER, k, t);
    }

    std::string enc_uppers = uppers;
    if (uppers.length() >= 2) {
      enc_uppers = coreFF1(uppers, is_encrypt, ALPHA_UPPER, k, t);
    }

    std::string enc_symbols = symbols;
    if (symbols.length() >= 2) {
      enc_symbols = coreFF1(symbols, is_encrypt, ALPHA_SYMBOL, k, t);
    }

    std::string result = "";
    int d_idx = 0, l_idx = 0, u_idx = 0, s_idx = 0;
    for (size_t i = 0; i < input.length(); ++i) {
      if(layout[i] == CharClass::DIGIT){
        result += enc_digits[d_idx++];
      }else if(layout[i] == CharClass::LOWER){
        result += enc_lowers[l_idx++];
      }else if(layout[i] == CharClass::UPPER){
        result += enc_uppers[u_idx++];
      }else if(layout[i] == CharClass::SYMBOL){
        result += enc_symbols[s_idx++];
      }else{
        result += input[i]; 
      }
    }
    return result;
  }

  /**
   * @brief FF1 格式保留加密核心实现（NIST SP 800-38G）
   * @param input 输入字符串
   * @param is_encrypt true=加密，false=解密
   * @param alphabet 字符集
   * @param local_key AES 密钥
   * @param local_tweak 微调值
   * @return 加密/解密结果
   */
  std::string coreFF1(const std::string& input, bool is_encrypt, const std::string& alphabet, 
                      const std::string& local_key, const std::string& local_tweak) {
    
    uint64_t radix = alphabet.length();
    size_t n = input.length();
    
    // NIST 强制要求 radix^n >= 100
    if (n < 2 || pow64(radix, n) < 100) {
      return input;
    }
    
    // 64位整数环境下的安全边界
    size_t max_safe_len = (radix == 10) ? 36 : 26; 
    if (n > max_safe_len) {
      return input;
    }

    AESEngine local_aes(local_key); 

    int u = std::floor(n / 2.0); 
    int v = n - u;
    
    std::string A = input.substr(0, u);
    std::string B = input.substr(u, v);

    // NIST 参数计算: b = ceil(ceil(v * log2(radix)) / 8)
    int b = std::ceil(std::ceil(v * std::log2(radix)) / 8.0);
    int d = 4 * std::ceil(b / 4.0) + 4;
    
    size_t t_len = local_tweak.length();

    // 构造 P 块
    uint8_t P[16] = {0};
    P[0] = 1;
    P[1] = 2;
    P[2] = 1;
    P[3] = static_cast<uint8_t>((radix >> 16) & 0xFF);
    P[4] = static_cast<uint8_t>((radix >> 8) & 0xFF);
    P[5] = static_cast<uint8_t>(radix & 0xFF);
    P[6] = 10;
    P[7] = static_cast<uint8_t>(u & 0xFF);
    P[8] = static_cast<uint8_t>((n >> 24) & 0xFF);
    P[9] = static_cast<uint8_t>((n >> 16) & 0xFF);
    P[10] = static_cast<uint8_t>((n >> 8) & 0xFF);
    P[11] = static_cast<uint8_t>(n & 0xFF);
    P[12] = static_cast<uint8_t>((t_len >> 24) & 0xFF);
    P[13] = static_cast<uint8_t>((t_len >> 16) & 0xFF);
    P[14] = static_cast<uint8_t>((t_len >> 8) & 0xFF);
    P[15] = static_cast<uint8_t>(t_len & 0xFF);

    // FF1 规定 10 轮循环
    for (int i = 0; i < 10; ++i) {
      int round = is_encrypt ? i : (9 - i);
      int m = (round % 2 == 0) ? u : v;
      
      // 构造 Q 块 (Tweak + Padding + Round + B)
      int padding_len = (-static_cast<int>(t_len) - b - 1) % 16;
      if (padding_len < 0) {
        padding_len += 16;
      }
      
      std::vector<uint8_t> Q(t_len + padding_len + 1 + b, 0);
      
      if (t_len > 0) {
        std::memcpy(Q.data(), local_tweak.data(), t_len);
      }
      
      Q[t_len + padding_len] = static_cast<uint8_t>(round & 0xFF);
      
      // 加密时 PRF 用 B，解密时 PRF 用 A
      std::string prf_input = is_encrypt ? B : A;
      uint64_t num_X = stringToUint64(prf_input, alphabet);
      
      for (int j = 0; j < b; ++j) {
        Q[Q.size() - 1 - j] = static_cast<uint8_t>((num_X >> (8 * j)) & 0xFF);
      }

      // AES-CBC-MAC 计算哈希
      uint8_t R[16] = {0};
      
      local_aes.encrypt(P, R); // 初始块
      
      for (size_t block = 0; block < Q.size(); block += 16) {
        for (int j = 0; j < 16; ++j) {
          R[j] ^= Q[block + j];
        }
        local_aes.encrypt(R, R);
      }

      // 提取 R 的前 d 个字节作为 S (利用 __int128 避免溢出)
      unsigned __int128 y_big = 0;
      for (int j = 0; j < std::min(d, 16); ++j) {
        y_big = (y_big * 256) + R[j];
      }

      uint64_t mod_val = pow64(radix, m);
      uint64_t y_val = static_cast<uint64_t>(y_big % mod_val);

      uint64_t num_target = stringToUint64(is_encrypt ? A : B, alphabet);
      uint64_t c = 0;
      
      if (is_encrypt) {
        c = (num_target + y_val) % mod_val;
      } else {
        c = (num_target + mod_val - y_val) % mod_val;
      }

      std::string C = uint64ToString(c, m, alphabet);

      // 交换左右半区
      if (is_encrypt) { 
        A = B; 
        B = C; 
      } else { 
        B = A; 
        A = C; 
      }
    }
    return A + B;
  }

  std::string key_;               // AES 密钥
  std::string tweak_;             // FF1 微调值
  bool ready_{false};             // 初始化完成标志
  std::shared_mutex mutex_;       
};

FPEInterface& getFPE() {
  static FPEImpl instance;
  return instance;
}

} // namespace FPE
} // namespace Common
} // namespace Extensions
} // namespace Envoy
