#pragma once

#include "envoy/server/factory_context.h"
#include "envoy/srhino_plugin_framework/v1_3_x/libs/net/net.h"
#include "source/common/srhino_plugin_framework/plugin_file_info.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
namespace Net {
class NetImpl : public Net {
public:
  NetImpl(Envoy::Server::Configuration::FactoryContext& factory_context,
          const PluginFileInfo& plugin_file_info)
      : factory_context_(factory_context), plugin_file_info_(plugin_file_info) {}

public:
  HttpCallSharedPtr createHttpCall(const std::string& cluster_name,
                                   const std::chrono::milliseconds& timeout,
                                   bool is_public_cluster = false) override;
  GrpcCallSharedPtr createGrpcCall(const std::string& cluster_name,
                                   const std::string& serivce_name,
                                   bool is_public_cluster = false) override;

private:
  Envoy::Server::Configuration::FactoryContext& factory_context_;
  const PluginFileInfo& plugin_file_info_;
};
} // namespace Net
} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework