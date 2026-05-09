#include "encoding_converter.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace EncodingConverter {

EncodingRe::EncodingRe(const re2::StringPiece& pattern) {
  re2::RE2::Options options;
  init(pattern, options);
}

EncodingRe::EncodingRe(const re2::StringPiece& pattern, re2::RE2::Options& options) {
  init(pattern, options);
}

void EncodingRe::init(const re2::StringPiece& pattern, re2::RE2::Options& options) {
  std::string gbk;

  // utf8
  regex_UTF8_ = std::shared_ptr<re2::RE2>(new re2::RE2(pattern, options));

  options.set_encoding(re2::RE2::Options::EncodingLatin1);

  // gbk
  if (convertToGBK(pattern, gbk)) {
    regex_GBK_ = std::shared_ptr<re2::RE2>(new re2::RE2(gbk, options));
  }

  //
  if ((std::find_if(pattern.begin(), pattern.end(), std::not1(std::ptr_fun(::isprint)))) ==
      pattern.end()) {
    regex_ISO_ = regex_UTF8_;
  } else {
    regex_ISO_ = std::shared_ptr<re2::RE2>(new re2::RE2(pattern, options));
  }
}

const std::shared_ptr<re2::RE2> EncodingRe::getEncodeRegex(EncodingType type) const {
  switch (type) {
  case EncodingType::GBK:
    return regex_GBK_;
    break;
  case EncodingType::ISO:
    return regex_ISO_;
    break;
  case EncodingType::UTF8:
    return regex_UTF8_;
    break;

  default:
    return regex_UTF8_;
    break;
  }

  return nullptr;
}

EncodingType EncodingRe::getEncodeType(const re2::StringPiece& content_type) {
  std::string str(content_type.data(), content_type.length());
  std::transform(str.begin(), str.end(), str.begin(), ::tolower);

  if (str.find("utf-8") != std::string::npos) {
    return EncodingType::UTF8;
  } else if (str.find("iso") != std::string::npos) {
    return EncodingType::ISO;
  } else if (str.find("gbk") != std::string::npos) {
    return EncodingType::GBK;
  }

  return EncodingType::UNKNOW;
}

bool EncodingRe::convertImpl(const re2::StringPiece& pattern, std::string& output,
                             const re2::StringPiece& from_encode,
                             const re2::StringPiece& to_encode) {
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

bool EncodingRe::convertToGBK(const re2::StringPiece& pattern, std::string& output) {
  return convertImpl(pattern, output, "UTF-8", "GBK");
}

LazyRegexSet::LazyRegexSet(const std::vector<std::string>& patterns) {
  re2::RE2::Options options;
  re2_set_.reset(new re2::RE2::Set(options, re2::RE2::UNANCHORED));
  for (size_t i = 0; i < patterns.size(); i++) {
    int id = re2_set_->Add(patterns[i], nullptr);
    if (id >= 0) {
      patterns_map_[id] = i;
    }
  }

  if (!re2_set_->Compile()) {
    // 编译失败，清空数据
    patterns_map_.clear();
    re2_set_ = nullptr;
  }
}

LazyRegexSet::LazyRegexSet(const std::vector<std::string>& patterns,
                           const re2::RE2::Options& options) {
  re2_set_.reset(new re2::RE2::Set(options, re2::RE2::UNANCHORED));

  for (size_t i = 0; i < patterns.size(); i++) {
    int id = re2_set_->Add(patterns[i], nullptr);
    if (id >= 0) {
      patterns_map_[id] = i;
    }
  }

  if (!re2_set_->Compile()) {
    // 编译失败，清空数据
    patterns_map_.clear();
    re2_set_ = nullptr;
  }
}

const std::unique_ptr<re2::RE2::Set>& LazyRegexSet::getRe2Set() const { return re2_set_; }

std::vector<uint32_t> LazyRegexSet::match(const re2::StringPiece& text) const {
  std::vector<uint32_t> ids{};

  auto& re2_set = getRe2Set();
  if (re2_set) {
    std::vector<int> v{};
    re2_set->Match(text, &v);

    for (auto id : v) {
      auto it = patterns_map_.find(id);
      if (it != patterns_map_.end()) {
        ids.push_back(it->second);
      }
    }
  }

  return ids;
}

EncodingRegexSet::EncodingRegexSet(const std::vector<std::string>& patterns)
    : utf8_re_set_(new LazyRegexSet(patterns)), gbk_re_set_([&patterns]() {
        std::vector<std::string> gbk;
        for (const auto it : patterns) {
          std::string val;
          if (EncodingConverter::EncodingRe::convertToGBK(it, val)) {
            gbk.push_back(val);
          }
        }

        re2::RE2::Options options;
        options.set_encoding(re2::RE2::Options::Encoding::EncodingLatin1);

        return LazyRegexSetPtr(new LazyRegexSet(gbk, options));
      }()) {}

std::vector<uint32_t> EncodingRegexSet::match(const re2::StringPiece& text,
                                              const EncodingConverter::EncodingType type) {
  std::vector<uint32_t> result;
  switch (type) {
  case EncodingConverter::EncodingType::UTF8:
    if (utf8_re_set_) {
      result = utf8_re_set_->match(text);
    }
    break;
  case EncodingConverter::EncodingType::GBK:
    if (gbk_re_set_) {
      result = gbk_re_set_->match(text);
    }
    break;
  case EncodingConverter::EncodingType::ISO:
    break;
  default:
    if (utf8_re_set_) {
      result = utf8_re_set_->match(text);
    }
    break;
  }

  return result;
}

} // namespace EncodingConverter
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy