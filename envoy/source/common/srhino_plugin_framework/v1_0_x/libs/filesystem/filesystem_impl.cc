#include "source/common/srhino_plugin_framework/v1_0_x/libs/filesystem/filesystem_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/filesystem/ini_file_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/filesystem/json_file_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/filesystem/plain_file_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/filesystem/xml_file_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/libs/filesystem/yaml_file_impl.h"

#include "source/common/srhino_plugin_framework/v1_0_x/context_impl.h"
#include "source/common/singleton/threadsafe_singleton.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace FileSystem {
IniFileSharedPtr FileSystemImpl::createIniFile(const std::string& /*resource_name*/) {
  return nullptr;
}

JsonFileSharedPtr FileSystemImpl::createJsonFile(const std::string& /*resource_name*/) {
  return nullptr;
}

PlainFileSharedPtr FileSystemImpl::createPlainFile(const std::string& resource_name) {
  return std::make_shared<PlainFileImpl>(getFilePath(resource_name));
}

XmlFileSharedPtr FileSystemImpl::createXmlFile(const std::string& /*resource_name*/) {
  return nullptr;
}

YamlFileSharedPtr FileSystemImpl::createYamlFile(const std::string& /*resource_name*/) {
  return nullptr;
}

const std::string FileSystemImpl::getFilePath(const std::string& resource_name) const {
  return manifest_->getFileResourcePath(resource_name);
}

} // namespace FileSystem
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework