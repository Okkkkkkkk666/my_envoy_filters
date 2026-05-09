#include "scanner.h"
#ifndef PCRE2_STATIC
#define PCRE2_STATIC
#endif

#ifndef PCRE2_CODE_UNIT_WIDTH
#define PCRE2_CODE_UNIT_WIDTH 8
#else
// #error PCRE2_CODE_UNIT_WIDTH was defined!
#endif
#include <set>
#include <assert.h>
#include <pcre2.h>
#include <iostream>
#include "likely.h"
namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {
static bool isThreeByteChinese(std::string_view sv, size_t pos = 0);
DataTagScanner::DataTagScanner(const DataTagDatabaseConstSharedPtr& data_tag_db)
    : data_tag_db_(data_tag_db) {
  if (data_tag_db_) {
    db_ptr_ = data_tag_db_.get();
    allocDatabaseScratch(*db_ptr_);
  }
  scratch_pcre_ = std::make_unique<PcreScratch>();
}

bool DataTagScanner::scan(const std::string_view& data, bool leftmost) {
  if (leftmost) {
    return scanComplete(data);
  } else {
    return scanFast(data);
  }
}

bool DataTagScanner::scanFast(const std::string_view& data) {
  assert(match_callback_);

  if (!db_ptr_) {
    ENVOY_LOG(error, "database pointer is null");
    return false;
  }
  auto hs_db = db_ptr_->nonLeftmostDb().get();
  auto scratch = scratch_.get();
  if (!hs_db || !scratch) {
    ENVOY_LOG(error, "database or scratch is null, hs_db: {}, scratch: {}",
              static_cast<const void*>(hs_db), static_cast<void*>(scratch));
    return false;
  }

  curr_scan_data_ = data;
  match_results_.clear();
  hs_scan(hs_db, data.data(), data.size(), 0, scratch, onMatchCb, this);

  // scan again for location 'from'
  locateFromPositions();

  processMatchResults();
  return true;
}

void DataTagScanner::locateFromPositions() {
  constexpr unsigned long long MAX_SCAN_FRONT_LENGTH = 128;
  match_results_swap_.clear();
  std::swap(match_results_, match_results_swap_);
  for (auto& [id, match_list] : match_results_swap_) {
    const auto& rule = db_ptr_->rule(id);

    if (rule.hasPcre() || !rule.master()) {
      match_results_.insert({id, std::move(match_list)});
      continue;
    } else {
      curr_id_ = id;
      const auto db = rule.master().get();
      for (auto pos = match_list.cbegin(); pos != match_list.cend(); ++pos) {
        const auto scan_to = pos->second;
        const auto scan_from =
            scan_to > MAX_SCAN_FRONT_LENGTH ? scan_to - MAX_SCAN_FRONT_LENGTH : 0;
        origin_offset_ = scan_from;

        hs_scan(db, curr_scan_data_.data() + scan_from, scan_to - scan_from, 0, scratch_.get(),
                onFromLocationFound, this);
      }
    }
  }
}

int DataTagScanner::onFromLocationFound([[maybe_unused]] unsigned int id, unsigned long long from,
                                        unsigned long long to, [[maybe_unused]] unsigned int flags,
                                        void* user_data) {
  auto* const self = static_cast<DataTagScanner*>(user_data);
  if (!self) {
    return 0;
  }

  const auto origin_from = self->origin_offset_ + from;
  const auto origin_to = self->origin_offset_ + to;
  const auto iter = self->match_results_.find(self->curr_id_);
  if (iter != self->match_results_.end()) {
    auto& list = iter->second;
    list.emplace_back(origin_from, origin_to);
  } else {
    std::list<std::pair<uint32_t, uint32_t>> list;
    list.emplace_back(origin_from, origin_to);
    self->match_results_.insert({self->curr_id_, std::move(list)});
  }
  return 0;
}

bool DataTagScanner::scanComplete(const std::string_view& data) {
  assert(match_callback_);

  if (!db_ptr_) {
    ENVOY_LOG(error, "database pointer is null");
    return false;
  }
  auto hs_db = db_ptr_->leftmostDb().get();
  auto scratch = scratch_.get();
  if (!hs_db || !scratch) {
    ENVOY_LOG(error, "database or scratch is null, hs_db: {}, scratch: {}",
              static_cast<const void*>(hs_db), static_cast<void*>(scratch));
    return false;
  }

  curr_scan_data_ = data;
  match_results_.clear();
  hs_scan(hs_db, data.data(), data.size(), 0, scratch, onMatchCb, this);

  processMatchResults();
  return true;
}

int DataTagScanner::onMatchCb(unsigned int id, unsigned long long from, unsigned long long to,
                              [[maybe_unused]] unsigned int flags, void* user_data) {
  auto* const self = static_cast<DataTagScanner*>(user_data);
  if (!self) {
    return 0;
  }

  const auto& rule = self->db_ptr_->rule(id);
  // check after stricture
  if (rule.hasAfter()) {
    // const uint32_t max_scan_length = rule.after().maxMatchLength();
    // const uint32_t scan_to = to + max_scan_length < self->curr_scan_data_.size()
    //                              ? to + max_scan_length
    //                              : self->curr_scan_data_.size();
    // const std::string_view after_data = self->curr_scan_data_.substr(to, scan_to - to);
    const std::string_view after_data = self->curr_scan_data_.substr(to);
    if (!rule.after().match(after_data, self->scratch_after_)) {
      return 0; // not match
    }
  }

  const auto iter = self->match_results_.find(id);
  if (iter != self->match_results_.end()) {
    auto& list = iter->second;
    auto& back = list.back();

    [[maybe_unused]] const uint64_t real_id = rule.id();
    if (back.first == from) {
      if (to > back.second) {
        const auto diff = to - back.second;
        if (diff == 1 || (diff == 3 && isThreeByteChinese(self->curr_scan_data_.data(), to - 2))) {
          ENVOY_LOG(trace, "block match position duplicated, id: {}, from: {}, last to: {}, to: {}",
                    real_id, from, back.second, to);
          back.second = to;
        } else {
          list.emplace_back(from, to);
        }
      } else {
        const auto diff = back.second - to;
        if (diff == 0 || diff == 1 ||
            (diff == 3 && isThreeByteChinese(self->curr_scan_data_.data(), to + 1))) {
          ENVOY_LOG(trace, "block match position duplicated, id: {}, from: {}, last to: {}, to: {}",
                    real_id, from, back.second, to);
          // if the diff is 0 or 1 or 3, it means that the match position is duplicated
          // discard the current match
        } else {
          list.emplace_back(from, to);
        }
      }
    } else {
      list.emplace_back(from, to);
    }
  } else {
    std::list<std::pair<uint32_t, uint32_t>> list;
    list.emplace_back(from, to);
    self->match_results_.insert({id, std::move(list)});
  }

  return 0;
}

void DataTagScanner::processMatchResults() {
  for (auto& [id, match_list] : match_results_) {
    const auto& rule = db_ptr_->rule(id);
    if (rule.hasPcre()) {
      pcreScan(rule, match_list);
    } else {
      validateMatchesWithRule(rule, match_list);
    }
  }
}

void DataTagScanner::validateMatchesWithRule(
    const DataTagRule& rule, const std::list<std::pair<uint32_t, uint32_t>>& results) const {
  const uint32_t id = rule.id();
  unsigned long long last_to{0};
  for (auto pos = results.cbegin(); pos != results.cend(); ++pos) {
    const auto& from = pos->first;
    const auto& to = pos->second;
    if (from > 0 && last_to > from) {
      ENVOY_LOG(trace, "block match position duplicated, id: {}, from: {}, last to: {}, to: {}",
                rule.id(), from, last_to, to);
      continue; // skip duplicated match
    }

    if (rule.hasContain()) {
      const std::string_view contain_data = curr_scan_data_.substr(from, to - from);
      bool match = true;
      for (const auto& c : rule.contains()) {
        if (!c.match(contain_data, scratch_)) {
          match = false;
          break;
        }
      }
      if (!match) {
        continue; // not match
      }
    }
    if (rule.hasBefore()) {
      const uint32_t max_scan_from = rule.before().maxMatchLength();
      const uint32_t scan_from = from > max_scan_from ? from - max_scan_from : 0;
      const std::string_view before_data = curr_scan_data_.substr(scan_from, from - scan_from);
      if (!rule.before().match(before_data, scratch_)) {
        continue; // not match
      }
    }

    ENVOY_LOG(trace, "rule matched id: {}, data: {}", id, curr_scan_data_.substr(from, to - from));

    last_to = to;
    match_callback_(id, from, to, user_data_);
  }
}

void DataTagScanner::pcreScan(const DataTagRule& rule,
                              const std::list<std::pair<uint32_t, uint32_t>>& results) const {
  const uint32_t id = rule.id();
  const PcrePattern* const pattern = rule.pcre_pattern();

  constexpr unsigned long long max_pcre_scan_front_len = 128;
  constexpr unsigned long long max_pcre_scan_back_len = 256;
  constexpr unsigned long long max_pcre_scan_len = 2048;

  unsigned long long pcre_scan_from{0};
  unsigned long long pcre_scan_to{0};
  // whether matched all the result areas
  bool finish{false};
  // unique matched result
  std::set<unsigned long long> unique_to;

  for (auto pos = results.cbegin(); !finish && pos != results.cend(); ++pos) {
    pcre_scan_from =
        pos->second > max_pcre_scan_front_len ? pos->second - max_pcre_scan_front_len : 0;
    pcre_scan_to = pos->second + max_pcre_scan_back_len < curr_scan_data_.length()
                       ? pos->second + max_pcre_scan_back_len
                       : curr_scan_data_.length();

    auto next = std::next(pos);
    while (next != results.cend()) {
      if (pcre_scan_to - pcre_scan_from < max_pcre_scan_len && pcre_scan_to > next->second) {
        ++pos;
        ++next;
        if (pos->second + max_pcre_scan_back_len < curr_scan_data_.length()) {
          pcre_scan_to = pos->second + max_pcre_scan_back_len;
        } else {
          pcre_scan_to = curr_scan_data_.length();
          finish = true;
          break;
        }
      } else {
        break;
      }
    }
    std::string_view pcre_scan_data =
        curr_scan_data_.substr(pcre_scan_from, pcre_scan_to - pcre_scan_from);

    auto pcre_match_info = pattern->matchGlobal(pcre_scan_data);
    for (const auto& from_to : pcre_match_info) {
      // init from and to
      const size_t ovector_from = from_to.first;
      const size_t ovector_to = from_to.second;
      unsigned long long from = pcre_scan_from + ovector_from;
      unsigned long long to = from + (ovector_to - ovector_from);

      // ensure that matched info does not duplicate
      // remove duplicate information based on the from and to values.
      if (unique_to.insert(to).second == false) {
        continue;
      }

      ENVOY_LOG(trace, "pcre matched id: {}, data: {}", id,
                curr_scan_data_.substr(from, to - from));

      // notify matched
      match_callback_(id, from, to, user_data_);
    }
  }
}

static bool isThreeByteChinese(std::string_view sv, size_t pos) {

  if (sv.size() < pos + 3)
    return false;

  unsigned char b1 = static_cast<unsigned char>(sv[pos]);
  unsigned char b2 = static_cast<unsigned char>(sv[pos + 1]);
  unsigned char b3 = static_cast<unsigned char>(sv[pos + 2]);

  // check UTF-8 three byte format
  if (b1 < 0xE0 || b1 > 0xEF)
    return false;
  if (b2 < 0x80 || b2 > 0xBF)
    return false;
  if (b3 < 0x80 || b3 > 0xBF)
    return false;

  //  convert Unicode code point
  uint32_t codepoint = ((b1 & 0x0F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);

  // judge whether  commom chinese characters U+4E00 ~ U+9FFF
  return codepoint >= 0x4E00 && codepoint <= 0x9FFF;
}

} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework