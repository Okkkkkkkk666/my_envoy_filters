#pragma once

#include <memory>
#include <string>
#include <vector>

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace ThreadLocal {
// FIXME(mq):
// 此接口按理应该在context中提供，但插件关联的集群名称都增加plugin_file_.cluster为前缀，
// 如：plugin_file_name_.cluster.cluster_name，但context接口未提供plugin_file_
class Cluster {
public:
  virtual ~Cluster() = default;

public:
  struct EndpointStatus {
    std::string address{};
    bool healthy{};
  };

public:
  /**
   *  偷瞄集群下一次负载均衡所选择的主机
   * @return 返回ip:port
   */
  virtual std::string peekAnotherHost() const = 0;

  /**
   * 获取所有节点健康状态
   * @return 返回所有节点状态
   */
  virtual std::vector<std::shared_ptr<EndpointStatus>> endpointHealthy() const = 0;
};
using ClusterSharedPtr = std::shared_ptr<Cluster>;
} // namespace ThreadLocal
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework