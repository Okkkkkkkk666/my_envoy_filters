#include "source/common/srhino_plugin_framework/v1_1_x/libs/net/net_impl.h"
#include "source/common/srhino_plugin_framework/v1_1_x/libs/net/http_call_impl.h"
#include "source/common/srhino_plugin_framework/v1_1_x/libs/net/grpc_call_impl.h"
#include "source/common/srhino_plugin_framework/v1_1_x/context_impl.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace Net {
HttpCallSharedPtr NetImpl::createHttpCall(const std::string& cluster_name,
                                          const std::chrono::milliseconds& timeout,
                                          bool is_public_cluster) {
  if (is_public_cluster) {
    std::string temp_cluster_name = cluster_name;
    return std::make_shared<HttpCallImpl>(factory_context_, std::move(temp_cluster_name), timeout);
  }
  return std::make_shared<HttpCallImpl>(
      factory_context_, std::format("{}.cluster.{}", plugin_file_info_.file_name_, cluster_name),
      timeout);
}

GrpcCallSharedPtr NetImpl::createGrpcCall(const std::string& cluster_name,
                                          const std::string& serivce_name, bool is_public_cluster) {
  if (is_public_cluster) {
    return std::make_shared<GrpcCallImpl>(factory_context_, cluster_name, serivce_name);
  }
  return std::make_shared<GrpcCallImpl>(
      factory_context_, std::format("{}.cluster.{}", plugin_file_info_.file_name_, cluster_name),
      serivce_name);
}
} // namespace Net
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework