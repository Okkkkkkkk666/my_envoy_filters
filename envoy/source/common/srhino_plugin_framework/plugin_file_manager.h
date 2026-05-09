#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <set>

#include "envoy/upstream/cluster_manager.h"
#include "envoy/server/admin.h"
#include "source/common/common/logger.h"
#include "source/common/srhino_plugin_framework/plugin_file.h"
#include "source/common/protobuf/protobuf.h"

namespace SrhinoPluginFramework {
class PluginFileManager : public Envoy::Logger::Loggable<Envoy::Logger::Id::runtime> {
public:
  PluginFileManager();
  ~PluginFileManager();

public:
  /**
   * 初始化
   */
  void init(Envoy::Upstream::ClusterManager* cm, Envoy::Server::Admin* admin);

  /**
   * 添加PluginFile实例
   * @param file_name 插件文件名 E.g. "www.srhino.com.acl.so.1.0.0"
   * @return PluginFileSharedPtr PluginFile实例共享指针
   */
  PluginFileSharedPtr addPluginFile(const std::string& file_name);

  /**
   * 删除PluginFile实例
   * @param file_name 插件文件名 E.g. "www.srhino.com.acl.so.1.0.0"
   */
  void delPluginFile(const std::string& file_name);

  /**
   * 根据插件文件名获取PluginFile实例
   * @param file_name 插件文件名 E.g. "www.srhino.com.acl.so.1.0.0"
   * @return PluginFileSharedPtr PluginFile实例共享指针
   */
  PluginFileSharedPtr getPluginFile(const std::string& file_name);

  /**
   * 跟现有PluginFile实例集合进行对比修改（取交集）。
   * 其内部实现通过组合调用addPluginFile、delPluginFile、getPluginFile
   * 来实现批量增加/删除的功能。
   * @param files 插件文件名集合
   */
  void merge(const std::set<std::string>& files);

private:
  Envoy::ProtobufTypes::MessagePtr dumpSrhinoPlugins() const;

private:
  Envoy::Upstream::ClusterManager* cm_{nullptr};
  Envoy::Server::Admin* admin_{nullptr};
  Envoy::Server::ConfigTracker::EntryOwnerPtr config_tracker_entry_;
  std::unordered_map<std::string, PluginFileSharedPtr> plugins_;
  bool init_{false};
};
} // namespace SrhinoPluginFramework