#pragma once

#include <memory>
#include <string>

#include "envoy/srhino_plugin_framework/v1_3_x/libs/thread_local/cluster.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
namespace ThreadLocal {
class ThreadLocal {
public:
  virtual ~ThreadLocal() = default;

public:
  /**
   * 获取与插件关联的cluster实例
   * @return 返回cluster实例指针
   */
  virtual ClusterSharedPtr createCluster(const std::string& cluster_name,
                                         bool is_public_cluster = false) const = 0;
};
using ThreadLocalSharedPtr = std::shared_ptr<ThreadLocal>;
} // namespace ThreadLocal
} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework