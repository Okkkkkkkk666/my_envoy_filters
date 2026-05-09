#pragma once

#include "envoy/srhino_plugin_framework/v1_4_x/libs/filesystem/xml_file.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace FileSystem {
// TODO: 提供XML文件的读写
// 包括:
// 根据字段名读取/写入值
// 枚举所有字段名
class XmlFileImpl final : public XmlFile {};
} // namespace FileSystem
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework