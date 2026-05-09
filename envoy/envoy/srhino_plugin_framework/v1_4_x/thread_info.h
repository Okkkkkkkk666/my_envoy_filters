#pragma once

#include <cstdint>
#include <string>

namespace SrhinoPluginFramework {
namespace v1_4_x {
class ThreadInfo {
public:
  virtual ~ThreadInfo() = default;

public:
  virtual int totalThreads() const = 0;
  virtual int currentThreadIndex() const = 0;
};
} // namespace v1_4_x
} // namespace SrhinoPluginFramework