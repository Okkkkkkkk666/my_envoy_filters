#pragma once

#include <memory>
#include <string>

#include "envoy/srhino_plugin_framework/v1_4_x/libs/public_lib/public_lib.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace PublicLib {

class PublicLibImpl : public PublicLib {
public:
  PublicLibCacheSharedPtr createPublicLibCache() override;
};
using PublicLibImplSharedPtr = std::shared_ptr<PublicLibImpl>;

} // namespace PublicLib
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework