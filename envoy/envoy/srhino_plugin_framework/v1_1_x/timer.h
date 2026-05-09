#pragma once

#include <chrono>
#include <functional>
#include <memory>

namespace SrhinoPluginFramework {
namespace v1_1_x {
class Timer {
public:
  virtual ~Timer() = default;

public:
  virtual void cancel() = 0;
  virtual void setInterval(std::chrono::milliseconds interval) = 0;
};

using TimerSharedPtr = std::shared_ptr<Timer>;
} // namespace v1_1_x
} // namespace SrhinoPluginFramework