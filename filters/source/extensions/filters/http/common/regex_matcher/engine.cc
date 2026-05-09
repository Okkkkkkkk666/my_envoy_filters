#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include "engine.h"
#include "huge.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace RegexMatcher {

int HyperscanEngine::real_init(const std::vector<ExpressionViewPtr>& expressions, std::vector<ExpressionView>& failed_exprs) {
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
    if (care_position_) {
      flags[i] |= HS_FLAG_SOM_LEFTMOST;
    }
  }

  hs_error_t err;
  hs_compile_error_t *compile_err;
  err = hs_compile_multi(exprs.data(), flags.data(), ids.data(), elements, full_mode, NULL, &db_, &compile_err);
  if (err == HS_COMPILER_ERROR) {
    if (compile_err->expression >= 0 && (static_cast<unsigned int>(compile_err->expression) < elements)) {
      failed_exprs.emplace_back(expressions[compile_err->expression]);
      ENVOY_LOG(error, "Compile error for expression #{} : {}", compile_err->expression, compile_err->message);
    } else {
      ENVOY_LOG(error, "Compile error: {}", compile_err->message);
    }
    hs_free_compile_error(compile_err);
    deinit();
    return -1;
  }

  if (!post_compile()) {
    ENVOY_LOG(error, "post compile process error");
    deinit();
    return -1;
  }
  return 0;
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
  if (compiledSize <= 0) {
    ENVOY_LOG(error, "hyperscan compile size error");
    return false;
  }

  total_threads_ = sysconf(_SC_NPROCESSORS_CONF);
  for (int i = 0; i < total_threads_; i++) {
    hs_scratch_t *scratch = nullptr;
    err = hs_alloc_scratch(db_, &scratch);
    if (err != HS_SUCCESS) {
      ENVOY_LOG(error, "Hyperscan alloc scratch failed");
      return false;
    } else {
      scratch_.push_back(scratch);
    }
  }

  return true;
}

bool HyperscanEngine::dumpDb(const std::string& path, const hs_database_t* out, size_t hash) {
  char* bytes = nullptr;
  size_t len = 0;
  hs_error_t err = hs_serialize_database(out, &bytes, &len);
  if (err != HS_SUCCESS) {
    ENVOY_LOG(error, "ERROR: hs_serialize_database() failed with error {}\n", err);
    return false;
  }

  std::ofstream db_file(path.c_str(), std::ios::out | std::ios::binary);
  if (!db_file) {
    ENVOY_LOG(error, "db {} open failed", path);
    return false;
  }

  db_file.write(reinterpret_cast<const char*>(&hash), sizeof(size_t));
  db_file.write(bytes, len);
  db_file.close();

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
  db_file.read(buf.get(), sizeof(size_t));
  struct HyperscanStoreInfo *store_info = reinterpret_cast<struct HyperscanStoreInfo *>(buf.get());
  if (store_info->hash != hash) {
    ENVOY_LOG(error, "db hash: {} error, hash: {}", store_info->hash, hash);
    db_file.close();
    return false;
  }
  // read the rest
  size_t db_size = file_size - sizeof(size_t);
  db_file.read(store_info->data, db_size);
  db_file.close();

  // deserialise
  hs_database_t* newdb;
  hs_error_t err = hs_deserialize_database(store_info->data, db_size, &newdb);
  if (err != HS_SUCCESS || !newdb) {
    ENVOY_LOG(error, "hyperscan deserialise failed: {}", err);
    return false;
  }
  *in = newdb;
  return true;
}

size_t HyperscanEngine::getExpresionsHash(const std::vector<ExpressionViewPtr>& expressions) {
  std::string md5_str;
  md5_str.append("\n"); md5_str.append(std::to_string(care_position_));
  md5_str.append("\n"); md5_str.append(std::to_string(mode_));
  for (const auto& it : expressions) {
    md5_str.append("\n"); md5_str.append(it->expr());
    md5_str.append("\n"); md5_str.append(std::to_string(it->id()));
    md5_str.append("\n"); md5_str.append(std::to_string(it->ignore_case()));
  }
  return std::hash<std::string>{}(md5_str);
}

int HyperscanEngine::init(const std::vector<ExpressionViewPtr>& expressions, std::vector<ExpressionView>& failed_exprs) {
  if (mode_ != HS_MODE_BLOCK && mode_ != HS_MODE_STREAM && mode_ != HS_MODE_VECTORED) {
    ENVOY_LOG(error, "mode must have one (and only one) of HS_MODE_BLOCK, HS_MODE_STREAM or HS_MODE_VECTORED");
    return -1;
  }

  if (!db_path_.empty()) {
    size_t hash = getExpresionsHash(expressions);

    //load from disk
    if (loadDb(db_path_, &db_, hash)) {
      ENVOY_LOG(debug, "load hyperscan db from disk success");
      if (!post_compile()) {
        deinit();
        return -1;
      }
    } else {
      ENVOY_LOG(error, "load hyperscan db from disk failed");

      if (0 != real_init(expressions, failed_exprs)) {
        ENVOY_LOG(error, "init hyperscan failed");
        return -1;
      }

      if (dumpDb(db_path_, db_, hash)) {
        ENVOY_LOG(debug, "dump hyperscan db to disk success");
      } else {
        ENVOY_LOG(error, "dump hyperscan db to disk failed");
      }
    }
  } else {
    return real_init(expressions, failed_exprs);
  }
  return 0;
}

int HyperscanEngine::deinit() {
  for (auto it : scratch_) {
    if (it) {
      hs_free_scratch(it);
    }
  }
  scratch_.clear();

  if (db_) {
    hs_free_database(db_);
    db_ = nullptr;
  }
  return 0;
}

HyperscanEngine::~HyperscanEngine()
{
  deinit();
}

static int HS_CDECL onMatch(unsigned int id, unsigned long long from, 
                          unsigned long long to, unsigned int, void *ctx) {
  std::vector<MatchResult> *ids_ptr = static_cast<std::vector<MatchResult>*>(ctx);
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
  for(auto& it : results) {
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

int HyperscanEngine::match(const std::string_view& data, std::vector<MatchResult>& results, int thread_index) {
  if (thread_index < 0 || thread_index >= total_threads_) {
    return -1;
  }
  hs_error_t err = hs_scan(db_, data.data(), data.length(), 0, scratch_[thread_index], onMatch, &results);
  if (err != HS_SUCCESS) {
    return -1;
  }

  // 贪婪模式，对重复匹配进行压缩
  greedyProcess(results);

  return 0;
}

int HyperscanEngine::match(const std::vector<const char*>& data, const std::vector<uint32_t>& length, std::vector<MatchResult>& results, int thread_index) {
  if (thread_index < 0 || thread_index >= total_threads_) {
    return -1;
  }
  hs_error_t err = hs_scan_vector(db_, &data[0], &length[0], data.size(), 0, scratch_[thread_index], onMatch, &results);
  if (err != HS_SUCCESS) {
    return -1;
  }

  // 贪婪模式，对重复匹配进行压缩
  greedyProcess(results);

  return 0;
}

int HyperscanEngine::streamOpen(StreamContext& ctx, uint32_t stream_id) {
  hs_open_stream(db_, 0, &ctx.id);
  if (!ctx.id)
    return -1;
  
  ctx.stream_id = stream_id;
  return 0;
}

int HyperscanEngine::streamScan(StreamContext& ctx, const std::string_view& data, std::vector<MatchResult>& results, int thread_index) {
  if (thread_index < 0 || thread_index >= total_threads_) {
    return -1;
  }
  hs_error_t err = hs_scan_stream(ctx.id, data.data(), data.length(), 0, scratch_[thread_index], onMatch, &results);
  if (err != HS_SUCCESS) {
    return -1;
  }
  return 0;
}

int HyperscanEngine::streamClose(StreamContext& ctx, std::vector<MatchResult>& results, int thread_index) {
  if (thread_index < 0 || thread_index >= total_threads_) {
    return -1;
  }
  hs_close_stream(ctx.id, scratch_[thread_index], onMatch, &results);
  greedyProcess(results);
  ctx.id = nullptr;
  return 0;
}

Re2Engine::Re2Engine(EncodingType encode) : encode_(encode) {
  re2_set_ = nullptr;
  expr_id_map_.clear();
}

int Re2Engine::init(const std::vector<ExpressionViewPtr>& expressions, std::vector<ExpressionView>& failed_exprs) {
  re2::RE2::Options options;
  if (encode_ != EncodingType::UTF8)
    options.set_encoding(re2::RE2::Options::Encoding::EncodingLatin1);
  re2_set_.reset(new re2::RE2::Set(options, re2::RE2::UNANCHORED));
  for (size_t i = 0; i < expressions.size(); i++) {
    re2::StringPiece pattern(expressions[i]->expr());
    if (expressions[i]->ignore_case()) {
      pattern = re2::StringPiece("(?i)" + expressions[i]->expr());
    }
    int id = re2_set_->Add(pattern, nullptr);
    if (id >= 0) {
      expr_id_map_[id] = expressions[i]->id();
    } else {
      failed_exprs.emplace_back(ExpressionView(expressions[i]));
      expr_id_map_.clear();
      re2_set_ = nullptr;
      return -1;
    }
  }

  if (!re2_set_->Compile()) {
    // 编译失败，清空数据
    expr_id_map_.clear();
    re2_set_ = nullptr;
    return -1;
  }

  return 0;
}

int Re2Engine::match(const std::string_view& data, std::vector<MatchResult>& results, int /*thread_index*/) {
  if (!re2_set_)
    return -1;

  std::vector<uint32_t> ids{};
  std::vector<int> v{};
  re2_set_->Match(data, &v);

  for (auto id : v) {
    auto it = expr_id_map_.find(id);
    if (it != expr_id_map_.end()) {
      results.emplace_back(MatchResult(it->second));
    }
  }

  return 0;
}

} // namespace RegexMatcher
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy