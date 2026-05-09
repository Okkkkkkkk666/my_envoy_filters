#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include "hyperscan_engine.h"
#include "huge.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace Regex {

static const std::string DB_SUFFIX(".sdb");
std::shared_mutex hs_db_file_mtx_;

bool HyperscanEngine::real_init(const std::vector<ExpressionViewSharedPtr>& expressions,
                                std::vector<ExpressionView>& failed_exprs) {
  if (expressions.empty()) {
    return false;
  }
  uint32_t full_mode = mode_;
  if (care_position_ && (mode_ == HS_MODE_STREAM)) {
    full_mode |= HS_MODE_SOM_HORIZON_SMALL;
  }
  unsigned int elements = expressions.size();
  std::vector<const char*> exprs(elements);
  std::vector<uint32_t> ids(elements);
  std::vector<uint32_t> flags(elements);
  for (size_t i = 0; i < elements; i++) {
    exprs[i] = expressions[i]->expr().c_str();
    ids[i] = expressions[i]->id();
    flags[i] = 0;
    if (expressions[i]->ignore_case()) {
      flags[i] |= HS_FLAG_CASELESS;
    }
    if (expressions[i]->utf_8_flag()) {
      flags[i] |= HS_FLAG_UTF8;
    }
    if (care_position_) {
      flags[i] |= HS_FLAG_SOM_LEFTMOST;
    }
  }

  hs_error_t err;
  hs_compile_error_t* compile_err;
  err = hs_compile_multi(exprs.data(), flags.data(), ids.data(), elements, full_mode, NULL, &db_,
                         &compile_err);
  if (err == HS_COMPILER_ERROR) {
    if (compile_err->expression >= 0 &&
        (static_cast<unsigned int>(compile_err->expression) < elements)) {
      failed_exprs.emplace_back(expressions[compile_err->expression]);
      ENVOY_LOG(error, "Compile error for expression #{} : {}", compile_err->expression,
                compile_err->message);
    } else {
      ENVOY_LOG(error, "Compile error: {}", compile_err->message);
    }
    hs_free_compile_error(compile_err);
    deinit();
    return false;
  }

  if (!post_compile()) {
    ENVOY_LOG(error, "post compile process error");
    deinit();
    return false;
  }
  return true;
}

bool HyperscanEngine::post_compile() {
  // copy the db into huge pages (where available) to reduce TLB pressure
  db_ = get_huge(db_);
  if (!db_) {
    ENVOY_LOG(error, "copy db into huge pages failed");
    return false;
  }

  size_t compiledSize;
  hs_error_t err = hs_database_size(db_, &compiledSize);
  if (err != HS_SUCCESS || compiledSize <= 0) {
    ENVOY_LOG(error, "hyperscan compile size error");
    return false;
  }

  return true;
}

hs_scratch_t* HyperscanEngine::getScratch() {
  {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    auto it = scratch_map_.find(std::this_thread::get_id());
    if (scratch_map_.end() != it) {
      return it->second;
    }
  }

  hs_scratch_t* scratch = nullptr;
  hs_error_t err = hs_alloc_scratch(db_, &scratch);
  if (err != HS_SUCCESS) {
    ENVOY_LOG(error, "Hyperscan alloc scratch failed");
    return nullptr;
  }
  // ENVOY_LOG(debug, "regex {} alloc scratch for thread: {}", reinterpret_cast<uintptr_t>(this), std::this_thread::get_id());
  std::unique_lock<std::shared_mutex> lock(mtx_);
  scratch_map_[std::this_thread::get_id()] = scratch;
  return scratch;
}

bool HyperscanEngine::dumpDb(const std::string& path, const hs_database_t* out, size_t hash) {
  char* bytes = nullptr;
  size_t len = 0;
  hs_error_t err = hs_serialize_database(out, &bytes, &len);
  if (err != HS_SUCCESS) {
    ENVOY_LOG(error, "db {} serialize failed with error {}", path, err);
    return false;
  }

  std::ofstream db_file(path.c_str(), std::ios::out | std::ios::binary);
  if (!db_file) {
    ENVOY_LOG(error, "db {} open failed", path);
    free(bytes);
    return false;
  }

  // write with lock
  {
    std::unique_lock<std::shared_mutex> lock(hs_db_file_mtx_);
    db_file.write(reinterpret_cast<const char*>(&hash), sizeof(size_t));
    db_file.write(bytes, len);
    db_file.close();
  }

  free(bytes);
  return true;
}

bool HyperscanEngine::loadDb(const std::string& path, hs_database_t** in, size_t hash) {
  struct stat info;
  if (0 != stat(path.c_str(), &info)) {
    ENVOY_LOG(error, "db {} open failed", path);
    return false;
  }

  size_t file_size = info.st_size;
  if (file_size <= sizeof(struct HyperscanStoreInfo)) {
    ENVOY_LOG(error, "db {} file size {} error", file_size);
    return false;
  }

  std::ifstream db_file(path.c_str(), std::ios::in | std::ios::binary);
  if (!db_file) {
    ENVOY_LOG(error, "db {} open failed", path);
    return false;
  }

  std::shared_ptr<char[]> buf(new char[file_size + 1]);
  struct HyperscanStoreInfo* store_info = reinterpret_cast<struct HyperscanStoreInfo*>(buf.get());
  size_t db_size = file_size - sizeof(size_t);

  // read with lock
  {
    std::shared_lock<std::shared_mutex> lock(hs_db_file_mtx_);
    db_file.read(buf.get(), sizeof(size_t));
    if (store_info->hash != hash) {
      ENVOY_LOG(error, "db {}, hash: {} error, expect hash: {}", path, store_info->hash, hash);
      db_file.close();
      return false;
    }
    // read the rest
    db_file.read(store_info->data, db_size);
    db_file.close();
  }

  // deserialise
  hs_database_t* newdb;
  hs_error_t err = hs_deserialize_database(store_info->data, db_size, &newdb);
  if (err != HS_SUCCESS || !newdb) {
    ENVOY_LOG(error, "db {}, deserialise failed: {}", path, err);
    return false;
  }
  *in = newdb;
  return true;
}

size_t HyperscanEngine::getExpresionsHash(const std::vector<ExpressionViewSharedPtr>& expressions) {
  std::string md5_str;
  md5_str.append("\n");
  md5_str.append(std::to_string(care_position_));
  md5_str.append("\n");
  md5_str.append(std::to_string(mode_));
  for (const auto& it : expressions) {
    md5_str.append("\n");
    md5_str.append(it->expr());
    md5_str.append("\n");
    md5_str.append(std::to_string(it->id()));
    md5_str.append("\n");
    md5_str.append(std::to_string(it->ignore_case()));
  }
  return std::hash<std::string>{}(md5_str);
}

bool HyperscanEngine::init(const std::vector<ExpressionViewSharedPtr>& expressions,
                           std::vector<ExpressionView>& failed_exprs) {
  if (mode_ != HS_MODE_BLOCK && mode_ != HS_MODE_STREAM && mode_ != HS_MODE_VECTORED) {
    ENVOY_LOG(
        error,
        "mode must have one (and only one) of HS_MODE_BLOCK, HS_MODE_STREAM or HS_MODE_VECTORED");
    return false;
  }

  if (!db_path_.empty()) {
    size_t hash = getExpresionsHash(expressions);
    std::string db_name = db_path_ + std::string("_") + std::to_string(hash) + DB_SUFFIX;

    // load from disk
    if (loadDb(db_name, &db_, hash)) {
      ENVOY_LOG(debug, "load db {} from disk success", db_name);
      if (!post_compile()) {
        deinit();
        return false;
      }
    } else {
      ENVOY_LOG(error, "load db {} from disk failed", db_name);

      if (!real_init(expressions, failed_exprs)) {
        ENVOY_LOG(error, "init hyperscan failed");
        return false;
      }

      if (dumpDb(db_name, db_, hash)) {
        ENVOY_LOG(debug, "dump db {} to disk success, hash: {}", db_name, hash);
      } else {
        ENVOY_LOG(error, "dump db {} to disk failed, hash: {}", db_name, hash);
      }
    }
  } else {
    return real_init(expressions, failed_exprs);
  }
  return true;
}

void HyperscanEngine::deinit() {
  for (auto& it : scratch_map_) {
    if (it.second) {
      hs_free_scratch(it.second);
    }
  }
  scratch_map_.clear();

  if (db_) {
    hs_free_database(db_);
    db_ = nullptr;
  }
}

HyperscanEngine::~HyperscanEngine() { deinit(); }

static int HS_CDECL onMatch(unsigned int id, unsigned long long from, unsigned long long to,
                            unsigned int, void* ctx) {
  std::vector<MatchResult>* ids_ptr = static_cast<std::vector<MatchResult>*>(ctx);
  if (!ids_ptr) {
    return -1;
  }
  ids_ptr->emplace_back(id, from, to);
  return 0;
}

/**
 * 将results按照贪婪匹配进行处理
 * 假设：results中元素已经按start排序
 */
bool HyperscanEngine::greedyProcess(std::vector<MatchResult>& results) {
  std::map<uint32_t, MatchResult*> temp;
  std::vector<MatchResult> new_results;
  for (auto& it : results) {
    auto last = temp.find(it.id());
    if (temp.end() != last) {
      if (it.start() == last->second->start()) {
        if (it.end() > last->second->end()) {
          last->second = &it;
        }
      } else {
        new_results.push_back(*(last->second));
        last->second = &it;
      }
    } else {
      temp[it.id()] = &it;
    }
  }
  for (auto it : temp) {
    new_results.push_back(*(it.second));
  }

  results = std::move(new_results);
  return true;
}

bool HyperscanEngine::match(const std::string_view& data, std::vector<MatchResult>& results) {
  if (!db_) {
    return false;
  }
  hs_scratch_t *scratch = getScratch();
  if (!scratch) {
    return false;
  }

  hs_error_t err = hs_scan(db_, data.data(), data.length(), 0, scratch, onMatch, &results);
  if (err != HS_SUCCESS) {
    return false;
  }

  // 贪婪模式，对重复匹配进行压缩
  return greedyProcess(results);
}

bool HyperscanEngine::match(const std::vector<const char*>& data,
                            const std::vector<uint32_t>& length,
                            std::vector<MatchResult>& results) {
  if (!db_) {
    return false;
  }
  hs_scratch_t *scratch = getScratch();
  if (!scratch) {
    return false;
  }

  hs_error_t err =
      hs_scan_vector(db_, &data[0], &length[0], data.size(), 0, scratch, onMatch, &results);
  if (err != HS_SUCCESS) {
    return false;
  }

  // 贪婪模式，对重复匹配进行压缩
  return greedyProcess(results);
}

bool HyperscanEngine::streamOpen(StreamContext& ctx, uint32_t stream_id) {
  if (!db_) {
    return false;
  }
  hs_stream_t* id = nullptr;
  hs_open_stream(db_, 0, &id);
  if (!id)
    return false;

  ctx.id = id;
  ctx.stream_id = stream_id;
  return true;
}

bool HyperscanEngine::streamScan(StreamContext& ctx, const std::string_view& data,
                                 std::vector<MatchResult>& results) {
  if (!db_) {
    return false;
  }
  hs_scratch_t *scratch = getScratch();
  if (!scratch) {
    return false;
  }

  hs_error_t err = hs_scan_stream(static_cast<hs_stream_t*>(ctx.id), data.data(), data.length(), 0,
                                  scratch, onMatch, &results);
  return (err == HS_SUCCESS);
}

bool HyperscanEngine::streamClose(StreamContext& ctx, std::vector<MatchResult>& results) {
  if (!db_) {
    return false;
  }
  hs_scratch_t *scratch = getScratch();
  if (!scratch) {
    return false;
  }
  hs_close_stream(static_cast<hs_stream_t*>(ctx.id), scratch, onMatch, &results);
  ctx.id = nullptr;
  return greedyProcess(results);
}

} // namespace Regex
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework