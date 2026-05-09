#pragma once

#include <string>
#include <string_view>

namespace SrhinoPluginFramework {
struct PluginFileInfo {
  // 插件文件名 E.g. "www.srhino.com.acl.so.1.0.0"
  std::string file_name_;

  // 插件文件的相对路径 E.g. "plugins/www.srhino.com/acl/acl.so.1.0.0"
  std::string path_;

  // 插件文件的相对目录 E.g. "plugins/www.srhino.com/acl"
  std::string_view dir_;

  // 插件开发者所在组织 E.g. "www.srhino.com"
  std::string domain_;

  // 插件名称 E.g. "acl"
  std::string name_;

  // 插件版本 E.g. "1.0.0"
  std::string version_;

  // 插件manifest清单文件的相对路径  E.g. "plugins/www.srhino.com/acl/acl.so.1.0.0.manifest.json"
  std::string manifest_path_;

  // 插件依赖的引擎版本  E.g. "1.0.0"
  std::string framework_min_required_;
};
} // namespace SrhinoPluginFramework