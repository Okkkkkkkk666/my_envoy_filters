#pragma once

#include <iconv.h>
#include <cstring>
#include <memory>
#include <algorithm>
#include <cctype>
#include "re2/re2.h"
#include "re2/set.h"

#include "source/common/common/non_copyable.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace EncodingConverter {

enum class EncodingType { UNKNOW, UTF8, GBK, ISO };

class EncodingRe {
public:
  EncodingRe(const re2::StringPiece& pattern);
  EncodingRe(const re2::StringPiece& pattern, re2::RE2::Options& options);
  void init(const re2::StringPiece& pattern, re2::RE2::Options& options);

  const std::shared_ptr<re2::RE2> getEncodeRegex(EncodingType) const;

  static EncodingType getEncodeType(const re2::StringPiece& content_type);
  static bool convertToGBK(const re2::StringPiece& pattern, std::string& output);

private:
  static bool convertImpl(const re2::StringPiece& pattern, std::string& output,
                          const re2::StringPiece& from_encode, const re2::StringPiece& to_encode);

  std::shared_ptr<re2::RE2> regex_GBK_{};
  std::shared_ptr<re2::RE2> regex_UTF8_{};
  std::shared_ptr<re2::RE2> regex_ISO_{};
};
using EncodingRePtr = std::unique_ptr<EncodingRe>;

class LazyRegexSet : NonCopyable {
public:
  LazyRegexSet(const std::vector<std::string>& patterns);
  LazyRegexSet(const std::vector<std::string>& patterns, const re2::RE2::Options& options);
  const std::unique_ptr<re2::RE2::Set>& getRe2Set() const;
  std::vector<uint32_t> match(const re2::StringPiece& text) const;

private:
  // key: re2::set内部的id, value 我们的规则id
  mutable std::map<uint32_t, uint32_t> patterns_map_{};
  mutable std::unique_ptr<re2::RE2::Set> re2_set_{};
};
using LazyRegexSetPtr = std::unique_ptr<LazyRegexSet>;

class EncodingRegexSet {
public:
  EncodingRegexSet(const std::vector<std::string>& patterns);

  std::vector<uint32_t> match(const re2::StringPiece& text, const EncodingConverter::EncodingType);

private:
  LazyRegexSetPtr utf8_re_set_;
  LazyRegexSetPtr gbk_re_set_;
};
using EncodingRegexSetPtr = std::unique_ptr<EncodingRegexSet>;

} // namespace regex
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
