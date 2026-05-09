#pragma once

#include "envoy/srhino_plugin_framework/v1_3_x/timer.h"
#include "envoy/event/dispatcher.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
class TimerImpl : public Timer {
public:
  TimerImpl(Envoy::Event::Dispatcher& dispatcher, std::chrono::milliseconds interval,
            std::function<bool(Timer& timer)> func);

public:
  void cancel() override;
  virtual void setInterval(std::chrono::milliseconds interval) override;

private:
  Envoy::Event::Dispatcher& dispatcher_;
  std::chrono::milliseconds interval_;
  std::function<bool(Timer& timer)> func_;
  Envoy::Event::TimerPtr timer_;
};
} // namespace v1_3_x
} // namespace SrhinoPluginFramework