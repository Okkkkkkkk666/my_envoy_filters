#include "string_utils.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
std::wstring stringToWString(const std::string& str) {
  std::setlocale(LC_ALL, "");
  std::wstring wstr(str.size(), L'\0');
  std::mbstowcs(&wstr[0], str.c_str(), str.size());
  return wstr;
}

std::string wstringToString(const std::wstring& wstr) {
  std::setlocale(LC_ALL, "");
  // 计算所需的字节数
  size_t bytesNeeded = std::wcstombs(nullptr, wstr.c_str(), 0);
  std::string str(bytesNeeded, '\0');
  // 转换
  std::wcstombs(&str[0], wstr.c_str(), bytesNeeded);
  return str;
}

size_t getWideCharCount(const std::string& input) {
  std::setlocale(LC_ALL, "");
  size_t wcStrLen = std::mbstowcs(NULL, input.c_str(), 0);

  if (wcStrLen == static_cast<size_t>(-1)) {
    return 0;
  }
  return wcStrLen;
}
} // namespace Replaces
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy