#pragma once

#include <cstdint>
#include <string>

namespace SrhinoPluginFramework {
namespace v1_2_x {
class ThreadInfo {
public:
  virtual ~ThreadInfo() = default;

public:
  virtual int total_threads() const = 0;
  virtual int current_thread_index() const = 0;
};
} // namespace v1_2_x
} // namespace SrhinoPluginFramework