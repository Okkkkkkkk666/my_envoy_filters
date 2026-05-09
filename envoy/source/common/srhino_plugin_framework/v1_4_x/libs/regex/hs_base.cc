#include "hs_base.h"

#include <assert.h>

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {

bool HsDatabase::compile(const std::vector<std::string>& exprs, const std::vector<unsigned>& ids,
                         const std::vector<unsigned>& flags) {
  if (hs_db_) {
    return false;
  }
  assert(exprs.size() == ids.size() && exprs.size() == flags.size());
  if (exprs.empty() || ids.empty() || flags.empty()) {
    // LOG(warn, "rules are empty, exprs size: {}, ids size: {}, flags size: {}", exprs.size(),
    //     ids.size(), flags.size());
    return false;
  }

  std::vector<const char*> exprs_ptrs;
  exprs_ptrs.reserve(exprs.size());
  for (const auto& expr : exprs) {
    exprs_ptrs.push_back(expr.c_str());
  }

  hs_compile_error_t* compile_err = nullptr;
  hs_database_t* db = nullptr;
  if (HS_SUCCESS != ::hs_compile_multi(exprs_ptrs.data(), flags.data(), ids.data(), exprs.size(),
                                       HS_MODE_BLOCK, nullptr, &db, &compile_err)) {
    // LOG(err, "index: {}, expression: {}, compile error: {}", compile_err->expression,
    //     exprs[compile_err->expression], compile_err->message);
    hs_free_compile_error(compile_err);
    return false;
  }
  hs_db_ = db;
  return true;
}

bool HsDatabase::compile(std::string_view expr, bool case_less, bool left_most) {
  if (hs_db_) {
    return false;
  }
  if (expr.empty()) {
    // LOG(warn, "rule expr is empty");
    return false;
  }

  unsigned flags = HS_FLAG_DOTALL | HS_FLAG_UTF8;
  if (case_less) {
    flags |= HS_FLAG_CASELESS;
  }
  if (left_most) {
    flags |= HS_FLAG_SOM_LEFTMOST;
  } else {
    flags |= HS_FLAG_SINGLEMATCH;
  }

  hs_compile_error_t* compile_err = nullptr;
  hs_database_t* db = nullptr;
  if (HS_SUCCESS != ::hs_compile(expr.data(), flags, HS_MODE_BLOCK, nullptr, &db, &compile_err)) {
    // LOG(err, "expression: {}, compile error: {}", expr, compile_err->message);
    hs_free_compile_error(compile_err);
    return false;
  }

  hs_db_ = db;
  return true;
}

} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework
