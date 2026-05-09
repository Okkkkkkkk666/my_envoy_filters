#pragma once

#include <cstdint>
#include <string>

#include <string.h>

#include "hex.hpp"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Utility {
class Url {
public:
  static std::string marshal(const void* data, uint32_t size) {
    std::string result;
    for (size_t i = 0; i < size; ++i) {
      const char& ch = static_cast<const char*>(data)[i];
      if (isAlpha(ch) || strchr("=!~*'()/?", ch)) {
        result += ch;
      } else if (ch == ' ') {
        result += '+';
      } else {
        result += '%';
        result += Hex::marshal(&ch, sizeof(ch));
      }
    }

    return result;
  }

  static std::string marshal(const std::string& data) { return marshal(data.data(), data.size()); }

  static std::string marshal(const std::string_view data) {
    return marshal(data.data(), data.size());
  }

  static std::string unmarshal(const void* data, size_t size) {
    std::string result;
    for (size_t i = 0; i < size; ++i) {
      const char& ch = static_cast<const char*>(data)[i];
      if (ch == '%') {
        if (i + 2 < size) {
          result += Hex::unmarshal(static_cast<const char*>(data) + i + 1, 2);
          i += 2;
        }
      } else if (ch == '+') {
        result += ' ';
      } else {
        result += ch;
      }
    }

    return result;
  }

  static std::string unmarshal(const std::string& data) {
    return unmarshal(data.data(), data.size());
  }

  static std::string unmarshal(const std::string_view data) {
    return unmarshal(data.data(), data.size());
  }

private:
  static bool isAlpha(char ch) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
      return true;
    }

    return false;
  }
};
} // namespace Utility
} // namespace v1_2_x
} // namespace SrhinoPluginFramework