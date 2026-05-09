#pragma once

#include <string_view>

#include "envoy/srhino_plugin_framework/v1_4_x/context.h"
#include "envoy/srhino_plugin_framework/v1_4_x/libs/net/grpc_call.h"
#include "envoy/srhino_plugin_framework/v1_4_x/libs/net/http_call.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace Net {
class Net {
protected:
  virtual ~Net() = default;

public:
  /**
   * 获取HTTP调用实例
   * @param cluster_name 集群名
   * @param timeout 超时时间
   * @param is_public_cluster 此集群是否为公共集群
   * @return 返回HTTP调用实例。在触发onInstall时由于尚未初始化完毕，此时会返回nullptr。
   */
  virtual HttpCallSharedPtr createHttpCall(const std::string& cluster_name,
                                           const std::chrono::milliseconds& timeout,
                                           bool is_public_cluster = false) = 0;

  /**
   * 获取Grpc调用实例
   * @param cluster_name 集群名
   * @param serivce_name grpc服务名
   * @param is_public_cluster 此集群是否为公共集群
   * @param timeout 调用超时时间
   * @return 返回Grpc调用实例。在触发onInstall时由于尚未初始化完毕，此时会返回nullptr。
   */
  virtual GrpcCallSharedPtr
  createGrpcCall(const std::string& cluster_name, const std::string& serivce_name,
                 const std::chrono::milliseconds& timeout = std::chrono::milliseconds(1000),
                 bool is_public_cluster = false) = 0;
};

using NetSharedPtr = std::shared_ptr<Net>;
} // namespace Net
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework