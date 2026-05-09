#pragma once

#include "envoy/server/factory_context.h"
#include "envoy/srhino_plugin_framework/v1_2_x/libs/thread_local/cluster.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace ThreadLocal {
class ClusterImpl : public Cluster {
public:
  ClusterImpl(Envoy::Server::Configuration::FactoryContext& factory_context,
              const std::string& cluster_name);

public:
  std::string peekAnotherHost() const override;

  std::vector<std::shared_ptr<EndpointStatus>> endpointHealthy() const override;

private:
  Envoy::Upstream::ThreadLocalCluster* cluster_{};
};
} // namespace ThreadLocal
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework