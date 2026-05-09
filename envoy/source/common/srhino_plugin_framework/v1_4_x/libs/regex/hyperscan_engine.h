#pragma once
#include <string>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <regex>
#include "source/common/common/logger.h"
#include "envoy/srhino_plugin_framework/v1_4_x/libs/regex/regex_matcher.h"
#include "pcre.h"
#include "likely.h"
#include "database.h"
#include "scanner.h"
#include <set>
#include <assert.h>
#include <pcre2.h>

#include "likely.h"
#include "database.h"
#ifdef __aarch64__
#include "third-party/hyperscan-aarch64/src/hs.h"
#else
#include "third-party/hyperscan/src/hs.h"
#endif

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {

struct HyperscanStoreInfo {
  size_t hash;  // 模式集校验hash
  char data[0]; // hyperscan serialize data
};
struct UserData {
  std::vector<MatchResult>* results;
  Pcre* pcre;
  std::string_view curr_match_data_;
  std::unordered_map<uint32_t, std::list<std::pair<uint32_t, uint32_t>>>* block_match_res;
};
class HyperscanEngine : public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  class ThreadLocalScanner {
  public:
    std::shared_ptr<DataTagScanner> getScanner(std::shared_ptr<const DataTagDatabase>& db) {
      auto it = scanners_.find(db.get());
      if (it != scanners_.end()) {
        return it->second;
      }

      auto scanner = std::make_shared<DataTagScanner>(db);
      scanners_[db.get()] = scanner;
      return scanner;
    }

  private:
    std::unordered_map<const DataTagDatabase*, std::shared_ptr<DataTagScanner>> scanners_;
  };

public:
  HyperscanEngine(bool care_position = false, uint32_t mode = HS_MODE_BLOCK)
      : care_position_(care_position), mode_(mode) {};
  HyperscanEngine(const std::string& db_path, bool care_position = false,
                  uint32_t mode = HS_MODE_BLOCK)
      : HyperscanEngine(care_position, mode) {
    db_path_ = db_path;
  };
  HyperscanEngine(const HyperscanEngine&) = delete;
  ~HyperscanEngine();
  bool init(const std::vector<ExpressionViewSharedPtr>& expressions,
            std::vector<ExpressionView>& failed_exprs);
  bool init(const std::vector<RuleConfig>& expressions, std::vector<RuleConfig>& failed_exprs);
  bool match(const std::string_view& data, std::vector<MatchResult>& results);
  bool match(const std::vector<const char*>& data, const std::vector<uint32_t>& length,
             std::vector<MatchResult>& results);
  bool pcreMatch(const std::string_view& data, std::vector<MatchResult>& results);
  bool pcreBlockMatch(const std::string_view& data, std::vector<MatchResult>& results);
  bool dataTagMatch(const std::string_view& data, std::vector<MatchResult>& results,
                    bool left_most);
  bool streamOpen(StreamContext& ctx, uint32_t stream_id = 0);
  bool streamScan(StreamContext& ctx, const std::string_view& data,
                  std::vector<MatchResult>& results);
  bool streamClose(StreamContext& ctx, std::vector<MatchResult>& results);
  void blockScan(const std::string_view& data, std::vector<MatchResult>& results);
  void pcreScan(uint32_t id, const PcrePattern* pattern,
                const std::list<std::pair<uint32_t, uint32_t>>& hs_results,
                std::vector<MatchResult>& results, const std::string_view& data, Pcre* pcre);

private:
  void deinit();
  bool greedyProcess(std::vector<MatchResult>& results);
  size_t getExpresionsHash(const std::vector<ExpressionViewSharedPtr>& expressions);
  bool real_init(const std::vector<ExpressionViewSharedPtr>& expressions,
                 std::vector<ExpressionView>& failed_exprs);
  bool post_compile();
  hs_scratch_t* getScratch();
  static bool loadDb(const std::string& path, hs_database_t** in, size_t hash);
  static bool dumpDb(const std::string& path, const hs_database_t* out, size_t hash);
  const PcrePatternList& getPcrePatternList() const { return pcre_pattern_list_; }
  static int matchCallback(unsigned int id, unsigned long long from, unsigned long long to,
                           unsigned int, void* user_data);

  static bool isPcre(const std::string& expression) {
    static std::regex is_pcre_pattern(R"(.*(?:\(\?=|\(\?!|\(\?<=|\(\?<!|\(\?>|\\\d|[?*+}]\+).*)");
    return std::regex_match(expression, is_pcre_pattern);
  }
  // 添加块扫描回调函数
  static int blockScanCallback(unsigned int id, unsigned long long from, unsigned long long to,
                               unsigned int, void* user_data);
  Pcre* getThreadLocalPcre() {
    static thread_local std::unique_ptr<Pcre> local_pcre;
    if (!local_pcre) {
      local_pcre = std::make_unique<Pcre>(pcre_pattern_list_);
    }
    return local_pcre.get();
  }
  void initThreadLocalScanner();

private:
  bool care_position_{false};
  uint32_t mode_{HS_MODE_BLOCK};
  hs_database_t* db_{nullptr};
  std::unordered_map<std::thread::id, hs_scratch_t*> scratch_map_;
  std::shared_mutex mtx_;
  // 支持从磁盘加载预编译数据库
  std::string db_path_;
  PcrePatternList pcre_pattern_list_;
  std::vector<MatchResult> pcre_match_results_;
  std::string_view curr_match_data_;
  std::unordered_map<uint32_t, std::list<std::pair<uint32_t, uint32_t>>> block_match_res_;
  DataTagDatabaseConstSharedPtr data_tag_db_;
};
using HyperscanEngineSharedPtr = std::shared_ptr<HyperscanEngine>;

} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework