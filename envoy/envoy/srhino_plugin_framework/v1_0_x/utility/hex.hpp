#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Utility {
static const std::string table_("0123456789abcdef");
class Hex {
public:
  static std::string marshal(const void* data, uint32_t size) {
    std::string result;
    result.resize(size * 2);
    char* pr = result.data();
    for (size_t i = 0; i < size; ++i) {
      pr[i * 2] = table_[*(reinterpret_cast<const unsigned char*>(data) + i) >> 4];
      pr[i * 2 + 1] = table_[*(reinterpret_cast<const unsigned char*>(data) + i) & 0x0f];
    }

    return result;
  }

  static std::string marshal(const std::string& data) { return marshal(data.data(), data.size()); }

  static std::string marshal(const std::string_view& data) {
    return marshal(data.data(), data.size());
  }

  static std::string unmarshal(const void* data, uint32_t size) {
    std::string temp(reinterpret_cast<const char*>(data), size);
    std::transform(temp.begin(), temp.end(), temp.begin(), ::tolower);
    const char* pch = temp.c_str();

    std::string result;
    result.resize(size / 2 + size % 2);
    char* pr = result.data();
    size_t len = 0;
    for (size_t i = 0; i < size; ++i) {
      auto pos = table_.find(pch[i]);
      if (pos == std::string::npos) {
        result.clear();
        break;
      }

      if (i % 2 == 0) {
        ++len;
        pr[len - 1] = static_cast<char>(pos << 4);
      } else {
        pr[len - 1] |= static_cast<char>(pos);
      }
    }

    return result;
  }

  static std::string unmarshal(const std::string& data) {
    return unmarshal(data.data(), data.size());
  }

  static std::string unmarshar(std::string_view data) {
    return unmarshal(data.data(), data.size());
  }
};

} // namespace Utility
} // namespace v1_0_x
} // namespace SrhinoPluginFramework