#pragma once

#include <memory>

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {
namespace FileSystem {
// TODO: 提供XML文件的读写
// 包括:
// 根据字段名读取/写入值
// 枚举所有字段名
class XmlFile {
public:
  virtual ~XmlFile() = default;
};

using XmlFileSharedPtr = std::shared_ptr<XmlFile>;
} // namespace FileSystem
} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework