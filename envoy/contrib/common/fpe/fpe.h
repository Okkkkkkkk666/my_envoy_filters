#pragma once
#include <string>

namespace Envoy {
namespace Extensions {
namespace Common {
namespace FPE {

enum class DataType {
  RAW_TEXT,   // 通用文本
  ID_CARD,    // 中国身份证 (保留末位，前17位数字打乱)
  PHONE,      // 手机号 (保留前3位号段，后8位打乱)
  EMAIL       // 邮箱 (保留 @domain，本地部分打乱)
};

class FPEInterface {
public:
  virtual ~FPEInterface() = default;
  
  virtual bool initFromConfigFile(const std::string& path) = 0;
  virtual bool initFromKey(const std::string& key, const std::string& tweak = "") = 0;
  virtual std::string encrypt(const std::string& plaintext, const std::string& context, DataType type = DataType::RAW_TEXT) = 0;
  virtual std::string decrypt(const std::string& ciphertext, const std::string& context, DataType type = DataType::RAW_TEXT) = 0;
};

// 返回全局单例实例
FPEInterface& getFPE();

} // namespace FPE
} // namespace Common
} // namespace Extensions
} // namespace Envoy
