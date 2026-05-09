#pragma once

#include <string>
#include <memory>

#include "ini_file.h"
#include "json_file.h"
#include "plain_file.h"
#include "xml_file.h"
#include "yaml_file.h"
#include "../../context.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace FileSystem {
class FileSystem {
protected:
  virtual ~FileSystem() = default;

public:
  /**
   * 创建操作INI文件的接口
   * @param resource_name manifest清单文件中指定的资源名
   */
  virtual IniFileSharedPtr createIniFile(const std::string& resource_name) = 0;

  /**
   * 创建操作JSON文件的接口
   * @param resource_name manifest清单文件中指定的资源名
   */
  virtual JsonFileSharedPtr createJsonFile(const std::string& resource_name) = 0;

  /**
   * 创建操作PLAIN文件的接口
   * @param resource_name manifest清单文件中指定的资源名
   */
  virtual PlainFileSharedPtr createPlainFile(const std::string& resource_name) = 0;

  /**
   * 创建操作XML文件的接口
   * @param resource_name manifest清单文件中指定的资源名
   */
  virtual XmlFileSharedPtr createXmlFile(const std::string& resource_name) = 0;

  /**
   * 创建操作YAML文件的接口
   * @param resource_name manifest清单文件中指定的资源名
   */
  virtual YamlFileSharedPtr createYamlFile(const std::string& resource_name) = 0;
};

using FileSystemSharedPtr = std::shared_ptr<FileSystem>;
} // namespace FileSystem
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework