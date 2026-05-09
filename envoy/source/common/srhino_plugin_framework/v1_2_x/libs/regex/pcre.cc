#include "pcre.h"

#ifndef PCRE2_STATIC
#define PCRE2_STATIC
#endif

#ifndef PCRE2_CODE_UNIT_WIDTH
#define PCRE2_CODE_UNIT_WIDTH 8
#else
// #error PCRE2_CODE_UNIT_WIDTH was defined!
#endif

#include <assert.h>
#include <pcre2.h>

#include "likely.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace Regex {
PcrePattern::PcrePattern(const std::string& pattern) : db_(nullptr) { compile(pattern); }

PcrePattern::~PcrePattern() {
  if (db_) {
    pcre2_code_free(reinterpret_cast<pcre2_code_8*>(db_));
    db_ = nullptr;
  }
}

bool PcrePattern::match(const std::string_view& subject, void* scratch, size_t& from,
                        size_t& to) const {
  assert(scratch);
  if (unlikely(!scratch)) {
    return false;
  }

  int rc = pcre2_match(reinterpret_cast<const pcre2_code_8*>(db_),
                       reinterpret_cast<const unsigned char*>(subject.data()), subject.length(), 0,
                       0, reinterpret_cast<pcre2_match_data_8*>(scratch), nullptr);
  if (unlikely(rc < 0)) {
    switch (rc) {
    case PCRE2_ERROR_NOMATCH:
      ENVOY_LOG(debug, "pcre no match: {}", subject);
      break;
    default:
      break;
    }
    return false;
  }

  assert(rc == 1);
  if (unlikely(rc == 0)) {
    ENVOY_LOG(error, "ovector was not big enough for captured substring", subject);
    return false;
  }

  auto ovector = pcre2_get_ovector_pointer(reinterpret_cast<pcre2_match_data_8*>(scratch));
  from = ovector[0];
  to = ovector[1];

  return true;
}

void PcrePattern::compile(const std::string& pattern) {
  int error_number;
  PCRE2_SIZE error_offset;
  db_ = pcre2_compile(reinterpret_cast<const unsigned char*>(pattern.c_str()), pattern.length(), 0,
                      &error_number, &error_offset, nullptr);
  if (unlikely(db_ == nullptr)) {
    char buffer[256];
    pcre2_get_error_message(error_number, reinterpret_cast<unsigned char*>(buffer), sizeof(buffer));
    ENVOY_LOG(warn, "pcre compile error: {}", buffer);
    return;
  }
}

void PcrePatternList::addPattern(const std::string& pattern, uint64_t id) {
  if (unlikely(pattern_map_.find(id) != pattern_map_.end())) {
    ENVOY_LOG(error, "add pattern failure! there has same id: {} {}", id, pattern);
    return;
  }

  pattern_map_.emplace(id, pattern);
}

const PcrePattern* PcrePatternList::getPattern(uint64_t id) const {
  const auto iter = pattern_map_.find(id);
  if (unlikely(iter == pattern_map_.end())) {
    return nullptr;
  }

  return &iter->second;
}

Pcre::Pcre(const PcrePatternList& pattern_list) : pattern_list_(pattern_list) {
  scratch_ = pcre2_match_data_create(1, nullptr);
}

Pcre::~Pcre() {
  if (scratch_) {
    pcre2_match_data_free(reinterpret_cast<pcre2_match_data*>(scratch_));
    scratch_ = nullptr;
  }
}

bool Pcre::match(uint64_t id, const std::string_view& subject, size_t& from, size_t& to) const {
  auto pattern = pattern_list_.getPattern(id);
  if (likely(pattern)) {
    return pattern->match(subject, scratch_, from, to);
  }

  return false;
}

bool Pcre::match(const PcrePattern* pattern, const std::string_view& subject, size_t& from,
                 size_t& to) const {
  if (likely(pattern)) {
    return pattern->match(subject, scratch_, from, to);
  }

  return false;
}
} // namespace Regex
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework