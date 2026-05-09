#pragma once

#include <stack>

#include "envoy/srhino_plugin_framework/v1_2_x/proto/manifest/manifest.pb.h"
#include "envoy/upstream/cluster_manager.h"
#include "envoy/server/admin.h"
#include "source/common/srhino_plugin_framework/plugin_file_info.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
class Manifest {
public:
  Manifest(const std::string& json, const PluginFileInfo& plugin_file_info,
           Envoy::Upstream::ClusterManager* cm);

public:
  /**
   * 校验manifest合法性
   * @param name 插件名字
   * @param error 如果校验失败，用来接收错误提示
   * @return 校验成功返回true，校验失败返回false
   */
  bool check(std::string& error);

  /**
   * 获取指定的资源
   * @param type 资源类型
   * @param name 资源名称
   * @return 获取的资源指针，如果获取失败则返回nullptr
   */
  const srhino_plugin_framework::v1_2_x::proto::manifest::resource::Resource*
  getResource(srhino_plugin_framework::v1_2_x::proto::manifest::resource::Resource::TypeCase type,
              const std::string& name) const;

  /**
   * 获取文件类型资源对应的文件的相对路径
   * @param name 资源名称
   * @return 相对于进程工作目录的相对路径
   */
  std::string getFileResourcePath(const std::string& name) const;

  const google::protobuf::RepeatedPtrField<
      srhino_plugin_framework::v1_2_x::proto::manifest::Controller>&
  getControllers() const;

private:
  using ManifestResource = srhino_plugin_framework::v1_2_x::proto::manifest::resource::Resource;
  using ManifestController = srhino_plugin_framework::v1_2_x::proto::manifest::Controller;

  // 从json字符串反序列化成srhino_plugin_framework::v1_2_x::proto::manifest::Manifest对象
  bool loadManifestFromJson(const std::string& json);

  // 校验基本信息，如插件名称、版本号等
  bool checkBaseInfo(std::string& error) const;

  // 生成资源名称
  std::string makeResourceName(ManifestResource::TypeCase type, const std::string& name) const;

  // 创建/回退所有申请的资源
  bool createResource(std::string& error);
  void revertCreateResource();

  // 创建/回退申请的cluster资源
  bool createCluster(const ManifestResource& res, std::string& error);
  void revertCreateCluster();

  // 创建/回退申请的file资源
  bool createFile(const ManifestResource& res, std::string& error);
  void revertCreateFile();

  // 创建/回退申请的listener资源
  bool createListener(const ManifestResource& res, std::string& error);
  void revertCreateListener();

  // 创建/回退申请的controller
  bool createController(std::string& error);
  void revertCreateController();

private:
  const PluginFileInfo& plugin_file_info_;
  Envoy::Upstream::ClusterManager* cm_;
  std::string prefix_source_name_;
  std::string load_error_;
  srhino_plugin_framework::v1_2_x::proto::manifest::Manifest manifest_;
  std::stack<std::string> revert_create_cluster_;
  Envoy::Server::ConfigTracker::EntryOwnerPtr admin_config_tracker_;
};

using ManifestSharedPtr = std::shared_ptr<Manifest>;
using ManifestConstSharedPtr = std::shared_ptr<const Manifest>;
} // namespace v1_2_x
} // namespace SrhinoPluginFramework