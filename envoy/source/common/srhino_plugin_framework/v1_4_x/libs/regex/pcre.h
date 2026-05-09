#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include "source/common/common/logger.h"
#include <stdint.h>
#include "hs_base.h"
#include <pcre2.h>
namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Regex {
class PcrePattern : public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  PcrePattern(const std::string& pattern, bool case_less);
  PcrePattern(const PcrePattern&) = delete;
  ~PcrePattern();

public:
  bool isValid() const { return db_ != nullptr; }
  bool match(const std::string_view& subject, void* scratch, size_t& from, size_t& to) const;
  std::vector<std::pair<size_t, size_t>> matchGlobal(const std::string_view& subject) const;
  std::vector<std::pair<size_t, size_t>> matchGlobal(const std::string_view& subject,
                                                     void* scratch) const;

private:
  void compile(const std::string& pattern, bool case_less);

private:
  void* db_;
};

class PcrePatternList : public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  void addPattern(const std::string& pattern, bool case_less, uint64_t id);
  const PcrePattern* getPattern(uint64_t id) const;

private:
  std::unordered_map<uint64_t, PcrePattern> pattern_map_;
};

class Pcre : public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  Pcre(const PcrePatternList& pattern_list);
  ~Pcre();

public:
  const PcrePattern* getPattern(uint64_t id) const { return pattern_list_.getPattern(id); }
  bool match(uint64_t id, const std::string_view& subject, size_t& from, size_t& to) const;
  bool match(const PcrePattern* pattern, const std::string_view& subject, size_t& from,
             size_t& to) const;
  std::vector<std::pair<size_t, size_t>> matchGlobal(uint64_t id,
                                                     const std::string_view& subject) const;
  std::vector<std::pair<size_t, size_t>> matchGlobal(const PcrePattern* pattern,
                                                     const std::string_view& subject) const;

private:
  const PcrePatternList& pattern_list_;
  mutable void* scratch_;
};

class PcreScratch : public NonCopyable {
public:
  PcreScratch();
  PcreScratch(PcreScratch&& other) = delete;
  PcreScratch& operator=(PcreScratch&& other) = delete;

  ~PcreScratch();

  void* get() const { return scratch_; }
  bool operator!() const { return scratch_ == nullptr; }
  explicit operator bool() const { return scratch_ != nullptr; }

private:
  void* scratch_{};
};
} // namespace Regex
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework