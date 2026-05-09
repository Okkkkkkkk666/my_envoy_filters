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

#include "likely.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {
PcrePattern::PcrePattern(const std::string& pattern, bool case_less) : db_(nullptr) {
  compile(pattern, case_less);
}

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
      ENVOY_LOG(trace, "pcre no match: {}", subject);
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

std::vector<std::pair<size_t, size_t>> PcrePattern::matchGlobal(const std::string_view& subject,
                                                                void* scratch) const {
  std::vector<std::pair<size_t, size_t>> result;
  assert(scratch);
  if (unlikely(!scratch)) {
    return result;
  }

  int rc = 0;
  size_t start_offset = 0;
  do {
    rc = pcre2_match(reinterpret_cast<const pcre2_code_8*>(db_),
                     reinterpret_cast<const unsigned char*>(subject.data()), subject.length(),
                     start_offset, 0, reinterpret_cast<pcre2_match_data_8*>(scratch), nullptr);
    if (rc == 1) {
      auto ovector = pcre2_get_ovector_pointer(reinterpret_cast<pcre2_match_data_8*>(scratch));
      result.emplace_back(std::make_pair(ovector[0], ovector[1]));
      start_offset = ovector[1] + 1;
    }
  } while (rc > 0);

  return result;
}

std::vector<std::pair<size_t, size_t>>
PcrePattern::matchGlobal(const std::string_view& subject) const {
  std::vector<std::pair<size_t, size_t>> result;

  if (unlikely(!isValid())) {
    ENVOY_LOG(error, "PCRE2: Pattern not compiled successfully");
    return result;
  }

  if (unlikely(subject.empty())) {
    return result;
  }

  // 为每次全局匹配创建专用的 match_data
  pcre2_match_data_8* match_data =
      pcre2_match_data_create_from_pattern_8(reinterpret_cast<const pcre2_code_8*>(db_), nullptr);

  if (!match_data) {
    ENVOY_LOG(error, "PCRE2: Failed to create match data");
    return result;
  }

  int rc = 0;
  size_t start_offset = 0;
  const size_t max_iterations = 1000;
  size_t iteration_count = 0;

  do {
    if (unlikely(++iteration_count > max_iterations)) {
      ENVOY_LOG(warn, "PCRE2: Too many iterations in matchGlobal");
      break;
    }

    if (unlikely(start_offset >= subject.length())) {
      break;
    }

    rc = pcre2_match_8(reinterpret_cast<const pcre2_code_8*>(db_),
                       reinterpret_cast<const unsigned char*>(subject.data()), subject.length(),
                       start_offset, 0, match_data, nullptr);

    if (rc > 0) {
      auto ovector = pcre2_get_ovector_pointer_8(match_data);
      size_t match_from = ovector[0];
      size_t match_to = ovector[1];

      if (match_from < subject.length() && match_to <= subject.length() && match_from < match_to) {
        result.emplace_back(match_from, match_to);
        start_offset = match_to;
      } else {
        ENVOY_LOG(warn, "PCRE2: Invalid match range: [{}, {}]", match_from, match_to);
        break;
      }
    } else if (rc == PCRE2_ERROR_NOMATCH) {
      break;
    } else if (rc < 0) {
      ENVOY_LOG(warn, "PCRE2: Match error: {}", rc);
      break;
    }
  } while (rc > 0);

  // 清理专用的 match_data
  pcre2_match_data_free_8(match_data);

  return result;
}

void PcrePattern::compile(const std::string& pattern, bool case_less) {
  int error_number;
  PCRE2_SIZE error_offset;
  auto flag = PCRE2_NO_AUTO_CAPTURE;
  if (case_less) {
    flag |= PCRE2_CASELESS;
  }
  db_ = pcre2_compile(reinterpret_cast<const unsigned char*>(pattern.c_str()), pattern.length(),
                      flag, &error_number, &error_offset, nullptr);
  if (unlikely(db_ == nullptr)) {
    char buffer[256];
    pcre2_get_error_message(error_number, reinterpret_cast<unsigned char*>(buffer), sizeof(buffer));
    ENVOY_LOG(warn, "pcre compile error: {}", buffer);
    return;
  }
}

void PcrePatternList::addPattern(const std::string& pattern, bool case_less, uint64_t id) {
  if (unlikely(pattern_map_.find(id) != pattern_map_.end())) {
    ENVOY_LOG(error, "add pattern failure! there has same id: {} {}", id, pattern);
    return;
  }
  pattern_map_.emplace(std::piecewise_construct, std::forward_as_tuple(id),
                       std::forward_as_tuple(pattern, case_less));
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

std::vector<std::pair<size_t, size_t>> Pcre::matchGlobal(uint64_t id,
                                                         const std::string_view& subject) const {
  auto pattern = pattern_list_.getPattern(id);
  if (likely(pattern)) {
    return pattern->matchGlobal(subject, scratch_);
  }

  return {};
}

std::vector<std::pair<size_t, size_t>> Pcre::matchGlobal(const PcrePattern* pattern,
                                                         const std::string_view& subject) const {
  if (likely(pattern)) {
    return pattern->matchGlobal(subject, scratch_);
  }

  return {};
}

PcreScratch::PcreScratch() : scratch_(pcre2_match_data_create(1, nullptr)) {}

PcreScratch::~PcreScratch() {
  if (scratch_) {
    pcre2_match_data_free(reinterpret_cast<pcre2_match_data*>(scratch_));
    scratch_ = nullptr;
  }
}

} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework