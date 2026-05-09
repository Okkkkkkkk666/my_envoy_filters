#pragma once
#include <list>
#include <pcre2.h>
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

#include "likely.h"
#include "database.h"
#include <iostream>
namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {

class DataTagScanner : public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
  using PcreScratchPtr = std::unique_ptr<PcreScratch>;
  using MatchResults = std::unordered_map<uint32_t, std::list<std::pair<uint32_t, uint32_t>>>;

public:
  DataTagScanner(const DataTagDatabaseConstSharedPtr& data_tag_db);
  ~DataTagScanner() = default;

  using MatchCallback = int (*)(uint32_t id, uint32_t from, uint32_t to, void* user_data);
  void registMatchCallback(MatchCallback cb, void* user_data) {
    match_callback_ = cb;
    user_data_ = user_data;
  }

  bool scan(const std::string_view& data, bool leftmost);

  MatchResults result() { return match_results_; }

protected:
  void allocDatabaseScratch(const DataTagDatabase& db) {
    db.allocScratch(scratch_);
    db.allocAfterScratch(scratch_after_);
  }

private:
  bool scanFast(const std::string_view& data);
  bool scanComplete(const std::string_view& data);

  static int onMatchCb(unsigned int id, unsigned long long from, unsigned long long to,
                       unsigned int flags, void* user_data);
  void processMatchResults();

  void validateMatchesWithRule(const DataTagRule& rule,
                               const std::list<std::pair<uint32_t, uint32_t>>& results) const;
  void pcreScan(const DataTagRule& rule,
                const std::list<std::pair<uint32_t, uint32_t>>& results) const;
  void locateFromPositions();
  static int onFromLocationFound(unsigned int id, unsigned long long from, unsigned long long to,
                                 unsigned int flags, void* user_data);

protected:
  const DataTagDatabase* db_ptr_{};

private:
  const DataTagDatabaseConstSharedPtr data_tag_db_;
  HsScratch scratch_{};
  HsScratch scratch_after_{};
  PcreScratchPtr scratch_pcre_{};


  MatchCallback match_callback_{nullptr};
  void* user_data_{nullptr};

  std::string_view curr_scan_data_;
  uint32_t origin_offset_{0};
  unsigned int curr_id_{};

  MatchResults match_results_;
  MatchResults match_results_swap_;
};
using DataTagScannerSharedPtr = std::shared_ptr<DataTagScanner>;

} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework