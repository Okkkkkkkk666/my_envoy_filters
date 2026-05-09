#pragma once

#include "envoy/srhino_plugin_framework/v1_3_x/thread_info.h"
#include "envoy/http/filter.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
class ThreadInfoImpl : public ThreadInfo {
public:
  ThreadInfoImpl(Envoy::Http::StreamFilterCallbacks* callbacks);

public:
  int totalThreads() const override;
  int currentThreadIndex() const override;

private:
  Envoy::Http::StreamFilterCallbacks* callbacks_;
  mutable int thread_index_;
};
} // namespace v1_3_x
} // namespace SrhinoPluginFramework