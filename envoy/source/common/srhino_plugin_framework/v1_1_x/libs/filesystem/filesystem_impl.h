#pragma once

#include "envoy/srhino_plugin_framework/v1_1_x/libs/filesystem/filesystem.h"
#include "source/common/srhino_plugin_framework/v1_1_x/manifest.h"
#include "source/common/srhino_plugin_framework/plugin_file_info.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace FileSystem {
class FileSystemImpl final : public FileSystem {
public:
  FileSystemImpl(const PluginFileInfo& plugin_file_info, ManifestConstSharedPtr manifest)
      : plugin_file_info_(plugin_file_info), manifest_(manifest) {}

public:
  IniFileSharedPtr createIniFile(const std::string& resource_name) override;
  JsonFileSharedPtr createJsonFile(const std::string& resource_name) override;
  PlainFileSharedPtr createPlainFile(const std::string& resource_name) override;
  XmlFileSharedPtr createXmlFile(const std::string& resource_name) override;
  YamlFileSharedPtr createYamlFile(const std::string& resource_name) override;

private:
  const std::string getFilePath(const std::string& resource_name) const;

private:
  const PluginFileInfo& plugin_file_info_;
  ManifestConstSharedPtr manifest_;
};
} // namespace FileSystem
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework