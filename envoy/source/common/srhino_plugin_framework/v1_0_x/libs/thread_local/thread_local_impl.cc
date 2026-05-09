#include "source/common/srhino_plugin_framework/v1_0_x/libs/thread_local/thread_local_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/thread_local/cluster_impl.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace ThreadLocal {
ClusterSharedPtr ThreadLocalImpl::createCluster(const std::string& cluster_name,
                                                bool is_public_cluster) const {
  if (is_public_cluster) {
    return std::make_shared<ClusterImpl>(factory_context_, cluster_name);
  }
  return std::make_shared<ClusterImpl>(
      factory_context_, std::format("{}.cluster.{}", plugin_file_info_.file_name_, cluster_name));
}
} // namespace ThreadLocal
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework