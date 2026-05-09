#pragma once

#include "envoy/srhino_plugin_framework/v1_2_x/libs/filesystem/json_file.h"

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {
namespace FileSystem {
// TODO: 提供JSON文件的读写
// 包括:
// 根据字段名读取/写入值
// 枚举所有字段名
class JsonFileImpl final : public JsonFile {};
} // namespace FileSystem
} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework