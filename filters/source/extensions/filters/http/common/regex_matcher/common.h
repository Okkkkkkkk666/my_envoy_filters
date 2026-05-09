#pragma once
#include <string>
#include <vector>
#include <memory>
#ifdef __aarch64__
#include "third-party/hyperscan-aarch64/src/hs.h"
#else
#include "third-party/hyperscan/src/hs.h"
#endif

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace RegexMatcher {

enum class RegexType { RegexAuto, RegexHyperscan, RegexRe2 };
enum EncodingType { UTF8, GBK, ISO, MAX, UNKNOWN = MAX };

class ExpressionView;
using ExpressionViewPtr = std::shared_ptr<ExpressionView>;
class ExpressionView {
public:
  ExpressionView() {}
  ExpressionView(const std::string& expr, uint32_t id, bool ignore_case = false)
      : expr_(expr), id_(id), ignore_case_(ignore_case) {}
  ExpressionView(const ExpressionViewPtr ev) {
    expr_ = ev->expr();
    id_ = ev->id();
    ignore_case_ = ev->ignore_case();
  }
  inline const std::string& expr() const { return expr_; }
  inline uint32_t id() const { return id_; }
  inline bool ignore_case() const { return ignore_case_; }

private:
  std::string expr_{};
  uint32_t id_{};
  bool ignore_case_{false}; /* 不区分大小写 */
};

class MatchResult {
public:
  MatchResult() {}
  MatchResult(uint32_t id, uint32_t start = 0, uint32_t end = 0)
      : id_(id), start_(start), end_(end) {}
  inline uint32_t id() const { return id_; }
  inline uint32_t start() const { return start_; }
  inline uint32_t end() const { return end_; }
  inline bool flag() const { return flag_; }
  inline void set_id(uint32_t id) { id_ = id; }
  inline void set_start(uint32_t start) { start_ = start; }
  inline void set_end(uint32_t end) { end_ = end; }
  inline void set_flag(bool flag) { flag_ = flag; }
  static bool cmp(const MatchResult& r1, const MatchResult& r2) { return r1.start() < r2.start(); }

private:
  uint32_t id_{}; /* expression id */
  /**
   *  start position.
   *  valid only in hyperscan mode
   */
  uint32_t start_{};
  /**
   *  end position.
   *  valid only in hyperscan mode
   */
  uint32_t end_{};
  bool flag_{true}; // 是否有效
};

class StreamContext {
public:
  hs_stream_t* id = nullptr; // hyperscan流模式下标识一个流的唯一ID，不要修改
  uint32_t stream_id = 0;    // 用户侧标识一个流的ID
  EncodingType encode = EncodingType::UTF8;
};

} // namespace RegexMatcher
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy