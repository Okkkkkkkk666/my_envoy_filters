#include "source/common/common/empty_string.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/thread_local/cluster_impl.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace ThreadLocal {
ClusterImpl::ClusterImpl(Envoy::Server::Configuration::FactoryContext& factory_context,
                         const std::string& cluster_name)
    : cluster_(factory_context.clusterManager().getThreadLocalCluster(cluster_name)) {}

std::string ClusterImpl::peekAnotherHost() const {
  if (!cluster_) {
    return Envoy::EMPTY_STRING;
  }
  const auto host = cluster_->loadBalancer().peekAnotherHost(nullptr);
  if (!host) {
    return Envoy::EMPTY_STRING;
  }
  return host->address()->asString();
}

std::vector<std::shared_ptr<Cluster::EndpointStatus>> ClusterImpl::endpointHealthy() const {
  std::vector<std::shared_ptr<EndpointStatus>> endpoints;

  if (cluster_) {
    const auto& hosts = cluster_->prioritySet().hostSetsPerPriority()[0]->hosts();
    for (const auto& host : hosts) {
      endpoints.emplace_back(std::make_shared<EndpointStatus>(
          host->address()->asString(),
          host->health() == Envoy::Upstream::Host::Health::Unhealthy ? false : true));
    }
  }
  return endpoints;
}
} // namespace ThreadLocal
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework