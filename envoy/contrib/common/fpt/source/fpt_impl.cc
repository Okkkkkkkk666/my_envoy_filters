#include "contrib/common/fpt/source/fpt.h"
#include <openssl/hmac.h>
#include <vector>

namespace Envoy {
namespace Extensions {
namespace Common {
namespace FPT {

namespace{

// 数字字符集 (0-9)
const std::string DICT_DIGITS = "0123456789";

//小写字母字符 (a-z)
const std::string DICT_LOWERS = "abcdefghijklmnopqrstuvwxyz";

// 大写字母字符 (A-Z)
const std::string DICT_UPPERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// 常见可打印符号字符集
const std::string DICT_SYMBOLS = " !\"#$%&()*+,-./:;<=>?@[]^_`{|}~";

const std::vector<std::string> DICT_CHINESE = {
    // --- 百家姓部分 ---
    "赵", "钱", "孙", "李", "周", "吴", "郑", "王", "冯", "陈",
    "褚", "卫", "蒋", "沈", "韩", "杨", "朱", "秦", "尤", "许",
    "何", "吕", "施", "张", "孔", "曹", "严", "华", "金", "魏",
    "陶", "姜", "戚", "谢", "邹", "喻", "柏", "水", "窦", "章",
    "云", "苏", "潘", "葛", "奚", "范", "彭", "郎", "鲁", "韦",
    "昌", "马", "苗", "凤", "花", "方", "俞", "任", "袁", "柳",
    "酆", "鲍", "史", "唐", "费", "廉", "岑", "薛", "雷", "贺",
    "倪", "汤", "滕", "殷", "罗", "毕", "郝", "邬", "安", "常",
    "乐", "于", "时", "傅", "皮", "卞", "齐", "康", "伍", "余",
    
    // --- 现代汉语高频字部分 ---
    "的", "一", "是", "了", "我", "不", "人", "在", "他", "有",
    "这", "个", "上", "们", "来", "到", "时", "大", "地", "为",
    "子", "中", "你", "说", "生", "国", "年", "着", "就", "那",
    "和", "要", "她", "出", "也", "得", "里", "后", "自", "以",
    "会", "家", "可", "下", "而", "过", "天", "去", "能", "对",
    "小", "多", "然", "于", "心", "学", "么", "之", "都", "好",
    "看", "起", "发", "当", "没", "成", "只", "如", "事", "把",
    "还", "用", "第", "样", "道", "想", "作", "种", "开", "美",
    "总", "从", "无", "情", "己", "面", "最", "女", "但", "现",
    "前", "些", "所", "同", "日", "手", "又", "行", "意", "动"
};

}// namespace

class FPTImpl : public FPTInterface {
public:
  std::string tokenize(const std::string& plaintext, const std::string& key) const override {
    if(plaintext.empty()){
      return "";
    }
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    HMAC(EVP_sha256(), key.c_str(), key.size(), 
            reinterpret_cast<const unsigned char*>(plaintext.c_str()), 
            plaintext.size(), hash, &hash_len);

    std::string token = "";
    size_t hash_idx = 0;
    size_t i = 0;

    while(i < plaintext.size()){
      auto c = plaintext[i];
      uint16_t rand_val = (hash[hash_idx % hash_len] << 8) | (hash[(hash_idx + 1) % hash_len]);
      hash_idx += 2;
      if(c >= '0' && c <= '9'){
        token += DICT_DIGITS[rand_val % DICT_DIGITS.size()];
        i += 1;
      }else if(c >= 'a' && c <= 'z'){
        token += DICT_LOWERS[rand_val % DICT_LOWERS.size()];
        i += 1; 
      }else if(c >= 'A' && c <= 'Z'){
        token += DICT_UPPERS[rand_val % DICT_UPPERS.size()];
        i += 1; 
      }else if(DICT_SYMBOLS.find(c) != std::string::npos){
        token += DICT_SYMBOLS[rand_val % DICT_SYMBOLS.size()];
        i += 1; 
      }else if ((c & 0xE0) == 0xE0 && (i + 2 < plaintext.length())){ // 简单判断中文字符（UTF-8 可能占用多字节）
        token += DICT_CHINESE[rand_val % DICT_CHINESE.size()];
        // 跳过一个中文字符的字节数（假设 UTF-8 编码，可能是 3 字节）
        i += 3;
      }else{
        token += c; // 其他字符保持不变
        i += 1;
      }
    }
    return token;
  }
};

FPTInterface& getFPT(){
  static FPTImpl instance;
  return instance;
}

} // namespace FPT
} // namespace Common
} // namespace Extensions
} // namespace Envoy