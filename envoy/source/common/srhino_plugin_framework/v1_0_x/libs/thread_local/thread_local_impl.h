#pragma once

#include "envoy/server/factory_context.h"
#include "source/common/srhino_plugin_framework/plugin_file_info.h"
#include "envoy/srhino_plugin_framework/v1_0_x/libs/thread_local/thread_local.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace ThreadLocal {
class ThreadLocalImpl : public ThreadLocal {
public:
  ThreadLocalImpl(Envoy::Server::Configuration::FactoryContext& factory_context,
                  const PluginFileInfo& plugin_file_info)
      : factory_context_(factory_context), plugin_file_info_(plugin_file_info) {}

public:
  ClusterSharedPtr createCluster(const std::string& cluster_name,
                                 bool is_public_cluster = false) const override;

private:
  Envoy::Server::Configuration::FactoryContext& factory_context_;
  const PluginFileInfo& plugin_file_info_;
};
} // namespace ThreadLocal
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework