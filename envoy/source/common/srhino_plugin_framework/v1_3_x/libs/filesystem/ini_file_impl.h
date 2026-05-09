#pragma once

#include "envoy/srhino_plugin_framework/v1_3_x/libs/filesystem/ini_file.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
namespace FileSystem {
// TODO: 提供INI文件的读写。
// 包括:
// 根据节名、字段名读取/写入值
// 枚举所有节名
// 枚举所有字段名
class IniFileImpl final : public IniFile {};
} // namespace FileSystem
} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework