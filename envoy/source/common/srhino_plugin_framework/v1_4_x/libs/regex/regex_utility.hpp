#pragma once
#include <iconv.h>
#include "re2/re2.h"
#include "re2/set.h"
#include "envoy/srhino_plugin_framework/v1_4_x/utility/http_parser.hpp"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {

using EncodingType = Utility::EncodingType;

static const std::map<EncodingType, std::string> EncodeMap = {
    {EncodingType::ISO_8859_1, "ISO-8859-1"},
    {EncodingType::UTF8, "UTF-8"},
    {EncodingType::GBK, "GBK"},
    {EncodingType::GB2312, "GB2312"}};

class RegexUtilities {
public:
  static bool convertImpl(const std::string_view& pattern, std::string& output,
                          const std::string_view& from_encode, const std::string_view& to_encode) {
    if (strcasecmp(from_encode.data(), to_encode.data()) == 0) {
      output = std::string(pattern);
      return true;
    }
    iconv_t cd = iconv_open(to_encode.data(), from_encode.data());
    if (cd == reinterpret_cast<iconv_t>(-1)) {
      return false;
    }
    char* inbuf = const_cast<char*>(pattern.data());
    size_t inbytesleft = pattern.length();

    size_t capacity = inbytesleft * 4;
    size_t outbytesleft = capacity;
    std::vector<char> buf(outbytesleft);
    char* outptr = buf.data();

    bool res = true;
    while (inbytesleft > 0) {
      if (iconv(cd, &inbuf, &inbytesleft, &outptr, &outbytesleft) == static_cast<size_t>(-1)) {
        res = false;
        break;
      }
    }

    iconv_close(cd);
    if (res) {
      output.assign(buf.data(), capacity - outbytesleft);
    }

    return res;
  }

  static std::string EncodingConvert(EncodingType from_type, EncodingType to_type,
                                     const std::string& pattern) {
    std::string new_pattern;
    if (convertImpl(pattern, new_pattern, EncodeMap.at(from_type), EncodeMap.at(to_type))) {
      return new_pattern;
    }
    return std::string();
  }

  static std::shared_ptr<re2::RE2> EncodingRe(bool ignore_case, const std::string& pattern,
                                              EncodingType type) {
    re2::RE2::Options options;
    options.set_case_sensitive(!ignore_case);
    if (type != EncodingType::UTF8) {
      options.set_encoding(re2::RE2::Options::EncodingLatin1);
    }
    return std::make_shared<re2::RE2>(pattern, options);
  }
};

} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework