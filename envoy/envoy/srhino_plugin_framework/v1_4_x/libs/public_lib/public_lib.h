#pragma once

#include <memory>
#include <string>

#include "envoy/srhino_plugin_framework/v1_4_x/libs/public_lib/cache.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace PublicLib {
class PublicLib {
public:
  virtual ~PublicLib() = default;

public:
  /**
   * 获取公共库缓存实例指针
   * @return PublicLibCacheSharedPtr
   */
  virtual PublicLibCacheSharedPtr createPublicLibCache() = 0;
};

using PublicLibSharedPtr = std::shared_ptr<PublicLib>;
} // namespace PublicLib
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework