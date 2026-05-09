#pragma once
#include <string>
#include <vector>

#ifdef __aarch64__
#include "third-party/hyperscan-aarch64/src/hs.h"
#else
#include "third-party/hyperscan/src/hs.h"
#endif

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {

class NonCopyable {
public:
  NonCopyable() = default;

  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;
  NonCopyable(NonCopyable&&) = default;
  NonCopyable& operator=(NonCopyable&&) = default;

  virtual ~NonCopyable() = default;
};

class HsDatabase : public NonCopyable {
public:
  HsDatabase() = default;
  HsDatabase(HsDatabase&& other) noexcept : hs_db_(other.hs_db_) { other.hs_db_ = nullptr; }
  HsDatabase& operator=(HsDatabase&& other) noexcept {
    if (this != &other) {
      std::swap(hs_db_, other.hs_db_);
    }
    return *this;
  }

  ~HsDatabase() {
    if (hs_db_) {
      hs_free_database(hs_db_);
      hs_db_ = nullptr;
    }
  }

  bool compile(const std::vector<std::string>& exprs, const std::vector<unsigned>& ids,
               const std::vector<unsigned>& flags);
  bool compile(std::string_view expr, bool case_less, bool left_most);
  const hs_database_t* get() const { return hs_db_; }
  bool operator!() const { return hs_db_ == nullptr; }
  explicit operator bool() const { return hs_db_ != nullptr; }

private:
  hs_database_t* hs_db_{nullptr};
};

class HsScratch : public NonCopyable {
public:
  HsScratch() = default;
  HsScratch(HsScratch&& other) noexcept : hs_scratch_(other.hs_scratch_) {
    other.hs_scratch_ = nullptr;
  }
  HsScratch& operator=(HsScratch&& other) noexcept {
    if (this != &other) {
      std::swap(hs_scratch_, other.hs_scratch_);
    }
    return *this;
  }

  ~HsScratch() {
    if (hs_scratch_) {
      hs_free_scratch(hs_scratch_);
      hs_scratch_ = nullptr;
    }
  }

  bool alloc(const HsDatabase& db) {
    return db ? (HS_SUCCESS == ::hs_alloc_scratch(db.get(), &hs_scratch_)) : true;
  }

  hs_scratch_t* get() const { return hs_scratch_; }
  bool operator!() const { return hs_scratch_ == nullptr; }
  explicit operator bool() const { return hs_scratch_ != nullptr; }

private:
  hs_scratch_t* hs_scratch_{nullptr};
};

} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework