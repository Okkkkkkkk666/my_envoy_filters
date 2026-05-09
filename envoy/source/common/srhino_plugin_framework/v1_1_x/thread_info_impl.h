#pragma once

#include "envoy/srhino_plugin_framework/v1_1_x/thread_info.h"
#include "envoy/http/filter.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
class ThreadInfoImpl : public ThreadInfo {
public:
  ThreadInfoImpl(Envoy::Http::StreamFilterCallbacks* callbacks);

public:
  int total_threads() const override;
  int current_thread_index() const override;

private:
  Envoy::Http::StreamFilterCallbacks* callbacks_;
  mutable int thread_index_;
};
} // namespace v1_1_x
} // namespace SrhinoPluginFramework