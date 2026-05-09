#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include "source/common/common/logger.h"
#include <stdint.h>

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace Regex {
class PcrePattern : public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  PcrePattern(const std::string& pattern);
  PcrePattern(const PcrePattern&) = delete;
  ~PcrePattern();

public:
  bool match(const std::string_view& subject, void* scratch, size_t& from, size_t& to) const;

private:
  void compile(const std::string& pattern);

private:
  void* db_;
};

class PcrePatternList : public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  void addPattern(const std::string& pattern, uint64_t id);
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

private:
  const PcrePatternList& pattern_list_;
  mutable void* scratch_;
};
} // namespace Regex
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework