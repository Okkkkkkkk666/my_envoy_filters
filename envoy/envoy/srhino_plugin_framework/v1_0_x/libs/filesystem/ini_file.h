#pragma once

#include <memory>

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace FileSystem {
// TODO: 提供INI文件的读写。
// 包括:
// 根据节名、字段名读取/写入值
// 枚举所有节名
// 枚举所有字段名
class IniFile {
public:
  virtual ~IniFile() = default;
};

using IniFileSharedPtr = std::shared_ptr<IniFile>;
} // namespace FileSystem
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework