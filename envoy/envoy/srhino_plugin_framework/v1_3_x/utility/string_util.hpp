#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Utility {
class StringUtil {

public:
  static constexpr std::string_view WhitespaceChars = " \t\f\v\n\r";
  static std::string_view ltrim(std::string_view source) {
    const std::string_view::size_type pos = source.find_first_not_of(WhitespaceChars);
    if (pos != std::string_view::npos) {
      source.remove_prefix(pos);
    } else {
      source.remove_prefix(source.size());
    }
    return source;
  }

  static std::string_view rtrim(std::string_view source) {
    const std::string_view::size_type pos = source.find_last_not_of(WhitespaceChars);
    if (pos != std::string_view::npos) {
      source.remove_suffix(source.size() - pos - 1);
    } else {
      source.remove_suffix(source.size());
    }
    return source;
  }

  static std::string_view trim(std::string_view source) { return ltrim(rtrim(source)); }

  template <typename M, typename N>
  static std::vector<M> strSplit(const M source, const N delimiters) {
    std::vector<M> result;
    auto token_start = source.cbegin();
    for (auto it = source.cbegin(); it != source.cend(); it++) {
      if (delimiters.find(*it) != delimiters.npos) {
        if (token_start == it) {
          result.push_back(std::string_view());
        } else {
          result.push_back(std::string_view(token_start, it - token_start));
        }
        token_start = it + 1;
      }
    }
    if (token_start == source.cend()) {
      result.push_back(std::string_view());
    } else {
      result.push_back(std::string_view(token_start, source.cend() - token_start));
    }
    return result;
  }

  static std::vector<std::string_view> splitToken(std::string_view source,
                                                  std::string_view delimiters,
                                                  bool keep_empty_string = true,
                                                  bool trim_whitespace = true) {
    std::vector<std::string_view> result = strSplit(source, delimiters);
    if (trim_whitespace) {
      for_each(result.begin(), result.end(), [](auto& v) { v = trim(v); });
    }
    if (!keep_empty_string) {
      for (auto it = result.begin(); it != result.end();) {
        if (it->empty()) {
          it = result.erase(it);
        } else {
          it++;
        }
      }
    }
    return result;
  }

  static std::string subspan(std::string_view source, size_t start, size_t end) {
    return std::string(source.data() + start, end - start);
  }
};

} // namespace Utility
} // namespace v1_3_x
} // namespace SrhinoPluginFramework