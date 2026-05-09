#pragma once

#include <memory>
#include "source/common/srhino_plugin_framework/file_name_matcher.h"

#define DECLARE_MANIFEST(major, minor, patch)                                                      \
  std::shared_ptr<SrhinoPluginFramework::v##major##_##minor##_x::Manifest>                          \
      manifest_v##major##_##minor##_x;

#define DECLARE_LIBS_FACTORY(major, minor, patch)                                                  \
  std::unique_ptr<SrhinoPluginFramework::v##major##_##minor##_x::Libs::FactoryImpl>                 \
      libs_factory_v##major##_##minor##_x;

#define CHECK_MANIFEST_BEGIN(framework_min_required)                                                \
  switch (SrhinoPluginFramework::FileNameMatcher::version(framework_min_required) & 0xFFFFC000) {

#define CHECK_MANIFEST_ENTRY(result, json, plugin_file_info, manifest, cluster_manager, error,     \
                             major, minor, patch)                                                  \
  case (major << 28) | (minor << 14): {                                                            \
    manifest.manifest_v##major##_##minor##_x =                                                     \
        std::make_unique<SrhinoPluginFramework::v##major##_##minor##_x::Manifest>(                  \
            json, plugin_file_info, cluster_manager);                                              \
    result = manifest.manifest_v##major##_##minor##_x->check(error);                               \
  } break;

#define CHECK_MANIFEST_END(framework_min_required, error)                                           \
  default:                                                                                         \
    error = "check manifest failure.framework_minimum_required version not supported: ";            \
    error += framework_min_required;                                                                \
    break;                                                                                         \
    }

#define CALL_INSTALL_BEGIN(framework_min_required)                                                  \
  switch (SrhinoPluginFramework::FileNameMatcher::version(framework_min_required) & 0xFFFFC000) {

#define CALL_INSTALL_ENTRY(factory, plugin_file_info, manifest, major, minor, patch)               \
  case (major << 28) | (minor << 14): {                                                            \
    factory->libs_factory_v##major##_##minor##_x =                                                 \
        std::make_unique<SrhinoPluginFramework::v##major##_##minor##_x::Libs::FactoryImpl>(         \
            plugin_file_info, manifest.manifest_v##major##_##minor##_x);                           \
    onInstall(factory->libs_factory_v##major##_##minor##_x.get());                                 \
  } break;

#define CALL_INSTALL_END(framework_min_required)                                                    \
  default:                                                                                         \
    break;                                                                                         \
    }

#define FILL_CONTROLLERS_BEGIN(framework_min_required)                                              \
  switch (SrhinoPluginFramework::FileNameMatcher::version(framework_min_required) & 0xFFFFC000) {

#define FILL_CONTROLLERS_ENTRY(installed_plugin, manifest, major, minor, patch)                    \
  case (major << 28) | (minor << 14): {                                                            \
    auto& controllers = manifest.manifest_v##major##_##minor##_x->getControllers();                \
    for (auto& c : controllers) {                                                                  \
      auto any = installed_plugin->add_controllers();                                              \
      any->PackFrom(c);                                                                            \
    }                                                                                              \
  } break;

#define FILL_CONTROLLERS_END()                                                                     \
  default:                                                                                         \
    break;                                                                                         \
    }

/**
 * step 1: 使用tools/srhino_plugin_framework_generator.sh脚本快速生成对应版本的源码
 */

/**
 * step 2: envoy/source/common/srhino_plugin_framework/BUILD 中添加新增版本的依赖
 */

/**
 * step 3: 包含对应版本的头文件
 *
 * E.g.
 * #include "source/common/srhino_plugin_framework/v1_0_x/libs/factory_impl.h"
 * #include "source/common/srhino_plugin_framework/v1_0_x/manifest.h"
 * #include "source/common/srhino_plugin_framework/v1_1_x/libs/factory_impl.h"
 * #include "source/common/srhino_plugin_framework/v1_1_x/manifest.h"
 * #include "source/common/srhino_plugin_framework/v1_2_x/libs/factory_impl.h"
 * #include "source/common/srhino_plugin_framework/v1_2_x/manifest.h"
 * #include "source/common/srhino_plugin_framework/v1_3_x/libs/factory_impl.h"
 * #include "source/common/srhino_plugin_framework/v1_3_x/manifest.h"
 * #include "source/common/srhino_plugin_framework/v1_4_x/libs/factory_impl.h"
 * #include "source/common/srhino_plugin_framework/v1_4_x/manifest.h"
 */
#include "source/common/srhino_plugin_framework/v1_0_x/libs/factory_impl.h"
#include "source/common/srhino_plugin_framework/v1_1_x/libs/factory_impl.h"
#include "source/common/srhino_plugin_framework/v1_2_x/libs/factory_impl.h"
#include "source/common/srhino_plugin_framework/v1_3_x/libs/factory_impl.h"
#include "source/common/srhino_plugin_framework/v1_4_x/libs/factory_impl.h"
#include "source/common/srhino_plugin_framework/v1_0_x/manifest.h"
#include "source/common/srhino_plugin_framework/v1_1_x/manifest.h"
#include "source/common/srhino_plugin_framework/v1_2_x/manifest.h"
#include "source/common/srhino_plugin_framework/v1_3_x/manifest.h"
#include "source/common/srhino_plugin_framework/v1_4_x/manifest.h"

/**
 * step 4: 使用DECLARE_MANIFEST宏来声明对应版本的manifest对象
 *
 * E.g.
 * struct MultiVersionManifest {
 *   DECLARE_MANIFEST(1, 0, x);
 *   DECLARE_MANIFEST(1, 1, x);
 *   DECLARE_MANIFEST(1, 2, x);
 *   DECLARE_MANIFEST(1, 3, x);
 *   DECLARE_MANIFEST(1, 4, x);
 * };
 */
namespace SrhinoPluginFramework {
struct MultiVersionManifest {
  DECLARE_MANIFEST(1, 0, x);
  DECLARE_MANIFEST(1, 1, x);
  DECLARE_MANIFEST(1, 2, x);
  DECLARE_MANIFEST(1, 3, x);
  DECLARE_MANIFEST(1, 4, x);
};
} // namespace SrhinoPluginFramework

/**
 * step 5: 使用DECLARE_LIBS_FACTORY宏来声明对应版本的LibsFactory对象
 *
 * E.g.
 * struct MultiVersionLibsFactory {
 *   DECLARE_LIBS_FACTORY(1, 0, x);
 *   DECLARE_LIBS_FACTORY(1, 1, x);
 *   DECLARE_LIBS_FACTORY(1, 2, x);
 *   DECLARE_LIBS_FACTORY(1, 3, x);
 *   DECLARE_LIBS_FACTORY(1, 4, x);
 * };
 */
namespace SrhinoPluginFramework {
struct MultiVersionLibsFactory {
  DECLARE_LIBS_FACTORY(1, 0, x);
  DECLARE_LIBS_FACTORY(1, 1, x);
  DECLARE_LIBS_FACTORY(1, 2, x);
  DECLARE_LIBS_FACTORY(1, 3, x);
  DECLARE_LIBS_FACTORY(1, 4, x);
};
} // namespace SrhinoPluginFramework

/**
 * step 6: 使用CHECK_MANIFEST_ENTRY宏校验对应版本的manifest
 *
 * E.g.
 * CHECK_MANIFEST_BEGIN(plugin_file_info.framework_min_required_)                            \
 * CHECK_MANIFEST_ENTRY(result, json, plugin_file_info, cluster_manager, error, 1, 0, x)    \
 * CHECK_MANIFEST_ENTRY(result, json, plugin_file_info, cluster_manager, error, 1, 1, x)    \
 * CHECK_MANIFEST_ENTRY(result, json, plugin_file_info, cluster_manager, error, 1, 2, x)    \
 * CHECK_MANIFEST_ENTRY(result, json, plugin_file_info, cluster_manager, error, 1, 3, x)    \
 * CHECK_MANIFEST_ENTRY(result, json, plugin_file_info, cluster_manager, error, 1, 4, x)    \
 * CHECK_MANIFEST_END(plugin_file_info.framework_min_required_, error)
 */
#define CHECK_MANIFEST(result, json, plugin_file_info, manifest, cluster_manager, error)           \
  CHECK_MANIFEST_BEGIN(plugin_file_info.framework_min_required_)                                    \
  CHECK_MANIFEST_ENTRY(result, json, plugin_file_info, manifest, cluster_manager, error, 1, 0, x)  \
  CHECK_MANIFEST_ENTRY(result, json, plugin_file_info, manifest, cluster_manager, error, 1, 1, x)  \
  CHECK_MANIFEST_ENTRY(result, json, plugin_file_info, manifest, cluster_manager, error, 1, 2, x)  \
  CHECK_MANIFEST_ENTRY(result, json, plugin_file_info, manifest, cluster_manager, error, 1, 3, x)  \
  CHECK_MANIFEST_ENTRY(result, json, plugin_file_info, manifest, cluster_manager, error, 1, 4, x)  \
  CHECK_MANIFEST_END(plugin_file_info.framework_min_required_, error)

/**
 * step 7: 使用CALL_INSTALL_ENTRY宏构造对应版本libs_factory并调用onInstall
 *
 * E.g.
 * CALL_INSTALL_BEGIN(plugin_file_info.framework_min_required_)              \
 * CALL_INSTALL_ENTRY(factory, plugin_file_info, manifest, , 0, x)          \
 * CALL_INSTALL_ENTRY(factory, plugin_file_info, manifest, 1, 1, x)         \
 * CALL_INSTALL_ENTRY(factory, plugin_file_info, manifest, 1, 2, x)         \
 * CALL_INSTALL_ENTRY(factory, plugin_file_info, manifest, 1, 3, x)         \
 * CALL_INSTALL_ENTRY(factory, plugin_file_info, manifest, 1, 4, x)         \
 * CALL_INSTALL_END(plugin_file_info.framework_min_required_)
 */
#define CALL_INSTALL(factory, plugin_file_info, manifest)                                          \
  CALL_INSTALL_BEGIN(plugin_file_info.framework_min_required_)                                      \
  CALL_INSTALL_ENTRY(factory, plugin_file_info, manifest, 1, 0, x)                                 \
  CALL_INSTALL_ENTRY(factory, plugin_file_info, manifest, 1, 1, x)                                 \
  CALL_INSTALL_ENTRY(factory, plugin_file_info, manifest, 1, 2, x)                                 \
  CALL_INSTALL_ENTRY(factory, plugin_file_info, manifest, 1, 3, x)                                 \
  CALL_INSTALL_ENTRY(factory, plugin_file_info, manifest, 1, 4, x)                                 \
  CALL_INSTALL_END(plugin_file_info.framework_min_required_)

/**
 * step 8: 使用FILL_CONTROLLERS_ENTRY宏填充admin管理接口的插件列表
 *
 * E.g.
 * FILL_CONTROLLERS_BEGIN(plugin_file_info.framework_min_required_)          \
 * FILL_CONTROLLERS_ENTRY(installed_plugin, manifest, 1, 0, x)              \
 * FILL_CONTROLLERS_ENTRY(installed_plugin, manifest, 1, 1, x)              \
 * FILL_CONTROLLERS_ENTRY(installed_plugin, manifest, 1, 2, x)              \
 * FILL_CONTROLLERS_ENTRY(installed_plugin, manifest, 1, 3, x)              \
 * FILL_CONTROLLERS_ENTRY(installed_plugin, manifest, 1, 4, x)              \
 * FILL_CONTROLLERS_END()
 */
#define FILL_CONTROLLERS(installed_plugin, plugin_file_info, manifest)                             \
  FILL_CONTROLLERS_BEGIN(plugin_file_info.framework_min_required_)                                  \
  FILL_CONTROLLERS_ENTRY(installed_plugin, manifest, 1, 0, x)                                      \
  FILL_CONTROLLERS_ENTRY(installed_plugin, manifest, 1, 1, x)                                      \
  FILL_CONTROLLERS_ENTRY(installed_plugin, manifest, 1, 2, x)                                      \
  FILL_CONTROLLERS_ENTRY(installed_plugin, manifest, 1, 3, x)                                      \
  FILL_CONTROLLERS_ENTRY(installed_plugin, manifest, 1, 4, x)                                      \
  FILL_CONTROLLERS_END()
