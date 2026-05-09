#pragma once

#include "envoy/srhino_plugin_framework/v1_2_x/libs/filesystem/yaml_file.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace FileSystem {
// TODO: 提供YAML文件的读写
// 包括:
// 根据字段名读取/写入值
// 枚举所有字段名
class YamlFileImpl final : public YamlFile {};
} // namespace FileSystem
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework