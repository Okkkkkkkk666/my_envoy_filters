#include "source/common/srhino_plugin_framework/v1_4_x/timer_impl.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
TimerImpl::TimerImpl(Envoy::Event::Dispatcher& dispatcher, std::chrono::milliseconds interval,
                     std::function<bool(Timer& timer)> func)
    : dispatcher_(dispatcher), interval_(interval), func_(func) {
  timer_ = dispatcher_.createTimer([&]() {
    if (func_) {
      bool is_continue = func_(*this);
      if (is_continue) {
        timer_->enableTimer(interval_);
      }
    }
  });
  timer_->enableTimer(interval_);
}

void TimerImpl::cancel() { timer_->disableTimer(); }

void TimerImpl::setInterval(std::chrono::milliseconds interval) { interval_ = interval; }
} // namespace v1_4_x
} // namespace SrhinoPluginFramework