#pragma once
#include <iostream>
#include <algorithm>
#include <vector>
#include <random>
#include <codecvt>
#include <locale>
#include <iconv.h>
#include <cstring>
#include <memory>
#include <cctype>
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
std::wstring stringToWString(const std::string& str);
std::string wstringToString(const std::wstring& wstr);
size_t getWideCharCount(const std::string& input);
} // namespace Replaces
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy