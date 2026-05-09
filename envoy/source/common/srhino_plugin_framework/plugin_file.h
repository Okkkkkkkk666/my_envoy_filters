#pragma once

#include <string_view>
#include <unordered_map>

#include "envoy/upstream/cluster_manager.h"
#include "source/common/common/logger.h"
#include "source/common/srhino_plugin_framework/multi_version_helper.h"
#include "source/common/srhino_plugin_framework/plugin_file_info.h"

namespace SrhinoPluginFramework {
struct MultiVersionLibsFactory;
class PluginFile : public Envoy::Logger::Loggable<Envoy::Logger::Id::runtime> {
public:
  /**
   * @param file_name E.g. "www.srhino.com.acl.so.1.0.0"
   */
  PluginFile(const std::string& file_name, Envoy::Upstream::ClusterManager* cm,
             Envoy::Server::Admin* admin);
  ~PluginFile();

public:
  using Handle = void*;

  // attributes
public:
  const PluginFileInfo& pluginFileInfo() const { return plugin_file_info_; }
  const MultiVersionManifest& manifest() const { return manifest_; }

  // so file's exports
public:
  void* createInstance(void* config);
  void destroyInstance(void* plugin);
  void onInstall(const void* libs_factory);
  void onUninstall();
  void* onBind(const char* config_json, uint32_t size, void* factory_context);
  void onUnbind(void* config);
  void* onBindRoute(const char* config_json, uint32_t size);
  void onUnbindRoute(void* config);
  const void* globalConfigSerialize(const char* config_json, uint32_t size);
  const void* perRouteConfigSerialize(const char* config_json, uint32_t size);
  const void* onControl(const char* method, uint32_t method_size, const char* url,
                        uint32_t url_size, const char* body, uint32_t body_size, void* config);
  void releaseResult(const void* result);

  // others:
public:
  bool isOpen() const { return handle_ != nullptr; }
  void addBinded(const std::string& name, void* config);
  void removeBinded(const std::string& name);
  const std::unordered_map<std::string, void*>& bindedList() const;

private:
  static bool readManifestAsJson(const std::string& path, std::string& error, std::string& json,
                                 std::string& framework_min_required);
  bool initPluginFileInfo(const std::string& file_name);
  bool versionCheck() const;
  void close();

private:
  PluginFileInfo plugin_file_info_;
  Handle handle_{nullptr};
  Envoy::Upstream::ClusterManager* cm_;
  Envoy::Server::Admin* admin_;
  std::unique_ptr<MultiVersionLibsFactory> multi_version_libs_factory_;
  MultiVersionManifest manifest_;
  std::unordered_map<std::string, void*> binded_list_;
};

using PluginFileSharedPtr = std::shared_ptr<PluginFile>;
} // namespace SrhinoPluginFramework