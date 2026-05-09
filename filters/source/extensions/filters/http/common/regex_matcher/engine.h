#pragma once
#include <string>
#include <vector>
#include <memory>
#include "source/common/common/logger.h"
#include "common.h"
#ifdef __aarch64__
#include "third-party/hyperscan-aarch64/src/hs.h"
#else
#include "third-party/hyperscan/src/hs.h"
#endif
#include "re2/re2.h"
#include "re2/set.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace RegexMatcher {

class Engine : public Logger::Loggable<Logger::Id::filter> {
public:
  Engine() {};
  virtual ~Engine() {};
  virtual int init(const std::vector<ExpressionViewPtr>& expressions, std::vector<ExpressionView>& failed_exprs) = 0;
  virtual int match(const std::string_view& data, std::vector<MatchResult>& results, int thread_index = 0) = 0;
};
using EnginePtr = std::shared_ptr<Engine>;

struct HyperscanStoreInfo {
  size_t hash; // 模式集校验hash 
  char data[0];    // hyperscan serialize data
};

class HyperscanEngine : public Engine {
public:
  HyperscanEngine(bool care_position = false, uint32_t mode = HS_MODE_BLOCK) : care_position_(care_position), mode_(mode) {};
  HyperscanEngine(const std::string& db_path, bool care_position = false, uint32_t mode = HS_MODE_BLOCK) : HyperscanEngine(care_position, mode) { db_path_ = db_path;};
  HyperscanEngine(const HyperscanEngine&) = delete;
  ~HyperscanEngine();
  int init(const std::vector<ExpressionViewPtr>& expressions, std::vector<ExpressionView>& failed_exprs);
  int match(const std::string_view& data, std::vector<MatchResult>& results, int thread_index = 0);
  int match(const std::vector<const char*>& data, const std::vector<uint32_t>& length, std::vector<MatchResult>& results, int thread_index = 0);
  int streamOpen(StreamContext& ctx, uint32_t stream_id = 0);
  int streamScan(StreamContext& ctx, const std::string_view& data, std::vector<MatchResult>& results, int thread_index = 0);
  int streamClose(StreamContext& ctx, std::vector<MatchResult>& results, int thread_index = 0);
private:
  int deinit();
  bool greedyProcess(std::vector<MatchResult>& results);
  size_t getExpresionsHash(const std::vector<ExpressionViewPtr>& expressions);
  int real_init(const std::vector<ExpressionViewPtr>& expressions, std::vector<ExpressionView>& failed_exprs);
  bool post_compile();
  static bool loadDb(const std::string& path, hs_database_t** in, size_t hash);
  static bool dumpDb(const std::string& path, const hs_database_t *out, size_t hash);
private:
  bool          care_position_{false};
  uint32_t      mode_{HS_MODE_BLOCK};
  hs_database_t *db_{nullptr};
  std::vector<hs_scratch_t *> scratch_;
  int           total_threads_{0};
  // 支持从磁盘加载预编译数据库
  std::string   db_path_;
};
using HyperscanEnginePtr = std::shared_ptr<HyperscanEngine>;

class Re2Engine : public Engine {
public:
  Re2Engine(EncodingType encode = EncodingType::UTF8);
  Re2Engine(const Re2Engine&) = delete;

  int init(const std::vector<ExpressionViewPtr>& expressions, std::vector<ExpressionView>& failed_exprs);
  int match(const std::string_view& data, std::vector<MatchResult>& results, int thread_index = 0);
private:
  EncodingType  encode_;
  std::unique_ptr<re2::RE2::Set> re2_set_;
  std::map<uint32_t, uint32_t> expr_id_map_;
};
using Re2EnginePtr = std::shared_ptr<Re2Engine>;


} // namespace RegexMatcher
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy