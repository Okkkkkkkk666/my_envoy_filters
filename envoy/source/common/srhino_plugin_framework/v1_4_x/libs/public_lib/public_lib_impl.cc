#include "source/common/srhino_plugin_framework/v1_4_x/libs/public_lib/cache_impl.h"
#include "source/common/srhino_plugin_framework/v1_4_x/libs/public_lib/public_lib_impl.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace PublicLib {

PublicLibCacheSharedPtr PublicLibImpl::createPublicLibCache() {
  return std::make_shared<PublicLibCacheImpl>();
}

} // namespace PublicLib
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework