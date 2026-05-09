#include <fstream>
#include <dlfcn.h>
#include <filesystem>

#include "source/common/srhino_plugin_framework/plugin_file.h"
#include "envoy/srhino_plugin_framework/v1_0_x/utility/proto_tools.hpp"

namespace SrhinoPluginFramework {
PluginFile::PluginFile(const std::string& file_name, Envoy::Upstream::ClusterManager* cm,
                       Envoy::Server::Admin* admin)
    : cm_(cm), admin_(admin) {
  if (!initPluginFileInfo(file_name)) {
    ENVOY_LOG(warn, "init plugin file info error");
    return;
  }

  // manifest校验
  bool check_ok = false;
  std::string error, json;
  if (readManifestAsJson(plugin_file_info_.manifest_path_, error, json,
                         plugin_file_info_.framework_min_required_)) {
    CHECK_MANIFEST(check_ok, json, plugin_file_info_, manifest_, cm_, error);
  }

  // 加载.so
  if (check_ok) {
    // 由于envoy排水机制，当插件卸载时，dlclose会延迟调用，当使用补丁升级时，由于文件路径没有修改，dlopen会直接使用envoy内存中的.so，导致旧插件无法卸载。
    // 此处创建不同名字的硬链接，确保加载时是唯一的，同时保留原本符号文件，方便查看core
    // 当热重启时，可能会存在新旧进程同时访问同一插件文件的情况，导致重命名失败，因此增加等待重试机制
    for (int retry_cnt = 0; retry_cnt < 5; ++retry_cnt) {
      try {
        if (std::filesystem::exists(plugin_file_info_.path_)) {
          static std::atomic<uint64_t> unique_id{0};
          std::string tmp_path =
              plugin_file_info_.path_ + "_tmp" + std::to_string(unique_id.fetch_add(1));
          std::filesystem::remove(tmp_path.c_str());
          std::filesystem::create_hard_link(plugin_file_info_.path_.c_str(), tmp_path.c_str());
          handle_ = ::dlopen(tmp_path.c_str(), RTLD_NOW | RTLD_LOCAL);
          if (!handle_) {
            ENVOY_LOG(warn, ::dlerror());
          }
          break;
        } else {
          ENVOY_LOG(warn, "plugin file {} not exist!", plugin_file_info_.path_);
        }
      } catch (const std::filesystem::filesystem_error& e) {
        ENVOY_LOG(warn, "rename plugin file failed: {}, retrying...", e.what());
        // 检查文件状态
        if (!std::filesystem::exists(plugin_file_info_.path_)) {
          ENVOY_LOG(warn, "so file disappeared: {}, retry again", plugin_file_info_.path_);
        }
      }
      // 等待后重试
      std::this_thread::sleep_for(std::chrono::milliseconds(100 * (retry_cnt + 1)));
      continue;
    }

    // 调用onInstall
    if (versionCheck()) {
      multi_version_libs_factory_ = std::make_unique<MultiVersionLibsFactory>();
      CALL_INSTALL(multi_version_libs_factory_, plugin_file_info_, manifest_);
    } else {
      close();
    }
  } else {
    ENVOY_LOG(warn, error);
  }
}

PluginFile::~PluginFile() {
  onUninstall();
  close();
}

void* PluginFile::createInstance(void* config) {
  using CreateInstanceFunc = void* (*)(void*);

  if (handle_) {
    CreateInstanceFunc func =
        reinterpret_cast<CreateInstanceFunc>(::dlsym(handle_, "createInstance"));
    if (func) {
      return func(config);
    } else {
      ENVOY_LOG(warn, ::dlerror());
    }
  }

  return nullptr;
}

void PluginFile::destroyInstance(void* plugin) {
  using DestroyInstanceFunc = void (*)(void*);

  if (handle_) {
    DestroyInstanceFunc func =
        reinterpret_cast<DestroyInstanceFunc>(::dlsym(handle_, "destroyInstance"));
    if (func) {
      func(plugin);
    } else {
      ENVOY_LOG(warn, ::dlerror());
    }
  }
}

void PluginFile::onInstall(const void* libs_factory) {
  using OnInstallFunc = void (*)(const void*);

  if (handle_) {
    OnInstallFunc func = reinterpret_cast<OnInstallFunc>(::dlsym(handle_, "onInstall"));
    if (func) {
      func(libs_factory);
    } else {
      ENVOY_LOG(warn, ::dlerror());
    }
  }
}

void PluginFile::onUninstall() {
  using OnUninstallFunc = void (*)();

  if (handle_) {
    OnUninstallFunc func = reinterpret_cast<OnUninstallFunc>(::dlsym(handle_, "onUninstall"));
    if (func) {
      func();
    } else {
      ENVOY_LOG(warn, ::dlerror());
    }
  }
}

void* PluginFile::onBind(const char* config_json, uint32_t size, void* factory_context) {
  using OnBindFun = void* (*)(const char*, uint32_t, void*);

  if (handle_) {
    OnBindFun func = reinterpret_cast<OnBindFun>(::dlsym(handle_, "onBind"));
    if (func) {
      return func(config_json, size, factory_context);
    } else {
      ENVOY_LOG(warn, ::dlerror());
    }
  }

  return nullptr;
}

void PluginFile::onUnbind(void* config) {
  using OnUnbindFunc = void (*)(void*);

  if (handle_) {
    OnUnbindFunc func = reinterpret_cast<OnUnbindFunc>(::dlsym(handle_, "onUnbind"));
    if (func) {
      func(config);
    } else {
      ENVOY_LOG(warn, ::dlerror());
    }
  }
}

void* PluginFile::onBindRoute(const char* config_json, uint32_t size) {
  using OnBindRouteFun = void* (*)(const char*, uint32_t);

  if (handle_) {
    OnBindRouteFun func = reinterpret_cast<OnBindRouteFun>(::dlsym(handle_, "onBindRoute"));
    if (func) {
      return func(config_json, size);
    } else {
      ENVOY_LOG(warn, ::dlerror());
    }
  }

  return nullptr;
}

void PluginFile::onUnbindRoute(void* config) {
  using OnUnbindRouteFunc = void (*)(void*);

  if (handle_) {
    OnUnbindRouteFunc func = reinterpret_cast<OnUnbindRouteFunc>(::dlsym(handle_, "onUnbindRoute"));
    if (func) {
      func(config);
    } else {
      ENVOY_LOG(warn, ::dlerror());
    }
  }
}

const void* PluginFile::globalConfigSerialize(const char* config_json, uint32_t size) {
  using GlobalConfigSerializeFunc = void* (*)(const char*, uint32_t);

  if (handle_) {
    GlobalConfigSerializeFunc func =
        reinterpret_cast<GlobalConfigSerializeFunc>(::dlsym(handle_, "globalConfigSerialize"));
    if (func) {
      return func(config_json, size);
    } else {
      ENVOY_LOG(warn, ::dlerror());
    }
  }

  return nullptr;
}

const void* PluginFile::perRouteConfigSerialize(const char* config_json, uint32_t size) {
  using PerRouteConfigSerializeFunc = void* (*)(const char*, uint32_t);

  if (handle_) {
    PerRouteConfigSerializeFunc func =
        reinterpret_cast<PerRouteConfigSerializeFunc>(::dlsym(handle_, "perRouteConfigSerialize"));
    if (func) {
      return func(config_json, size);
    } else {
      ENVOY_LOG(warn, ::dlerror());
    }
  }

  return nullptr;
}

const void* PluginFile::onControl(const char* method, uint32_t method_size, const char* url,
                                  uint32_t url_size, const char* body, uint32_t body_size,
                                  void* config) {
  using OnControlFunc =
      void* (*)(const char*, uint32_t, const char*, uint32_t, const char*, uint32_t, void*);

  if (handle_) {
    OnControlFunc func = reinterpret_cast<OnControlFunc>(::dlsym(handle_, "onControl"));
    if (func) {
      return func(method, method_size, url, url_size, body, body_size, config);
    } else {
      ENVOY_LOG(warn, ::dlerror());
    }
  }

  return nullptr;
}

void PluginFile::releaseResult(const void* result) {
  using ReleaseResultFunc = void (*)(const void*);

  if (handle_) {
    ReleaseResultFunc func = reinterpret_cast<ReleaseResultFunc>(::dlsym(handle_, "releaseResult"));
    if (func) {
      func(result);
    } else {
      ENVOY_LOG(warn, ::dlerror());
    }
  }
}

void PluginFile::addBinded(const std::string& name, void* config) { binded_list_[name] = config; }

void PluginFile::removeBinded(const std::string& name) { binded_list_.erase(name); }

const std::unordered_map<std::string, void*>& PluginFile::bindedList() const {
  return binded_list_;
}

bool PluginFile::readManifestAsJson(const std::string& path, std::string& error, std::string& json,
                                    std::string& framework_min_required) {
  std::ifstream ifs;
  ifs.open(path, std::ios::binary);
  if (ifs.is_open()) {
    ifs.seekg(0, ifs.end);
    size_t data_len = static_cast<size_t>(ifs.tellg());
    ifs.seekg(0, ifs.beg);

    json.resize(data_len);
    if (!ifs.read(json.data(), json.size())) {
      json.resize(ifs.gcount());
    }

    ifs.close();
  } else {
    error = fmt::format("can not open manifest: {}", path);
    return false;
  }

  // 读取framework_minimum_required字段值
  std::size_t field_pos = json.find("\"framework_minimum_required\"");
  if (field_pos != std::string::npos) {
    std::size_t spliter_pos = json.find(":", field_pos);
    if (spliter_pos != std::string::npos) {
      std::size_t value_begin_pos = json.find("\"", spliter_pos);
      std::size_t value_end_pos = json.find("\"", value_begin_pos + 1);
      if (value_begin_pos != std::string::npos && value_end_pos != std::string::npos) {
        framework_min_required =
            json.substr(value_begin_pos + 1, value_end_pos - value_begin_pos - 1);
      }
    }
  }
  if (framework_min_required.empty()) {
    error = "load manifest failure: can not read framework_minimum_required field";
    return false;
  }

  return true;
}

bool PluginFile::initPluginFileInfo(const std::string& file_name) {
  static constexpr char base_path[] = "plugins";
  static constexpr char manifest_file_name[] = "manifest.json";

  plugin_file_info_.file_name_ = file_name;

  bool ret = FileNameMatcher::match(file_name, plugin_file_info_.domain_, plugin_file_info_.name_,
                                    plugin_file_info_.version_);
  if (!ret) {
    return false;
  }

  plugin_file_info_.path_ =
      std::format("{}/{}/{}/{}", base_path, plugin_file_info_.domain_, plugin_file_info_.name_,
                  file_name.substr(plugin_file_info_.domain_.size() + 1));

  auto pos = plugin_file_info_.path_.rfind('/');
  if (pos != std::string::npos) {
    plugin_file_info_.dir_ = std::string_view({plugin_file_info_.path_.data(), pos});
  }

  plugin_file_info_.manifest_path_ = plugin_file_info_.path_ + "." + manifest_file_name;

  return true;
}

bool PluginFile::versionCheck() const {
  using VersionFunc = const char* (*)();

  if (handle_) {
    VersionFunc func = reinterpret_cast<VersionFunc>(::dlsym(handle_, "version"));
    if (func) {
      if (plugin_file_info_.version_ == func()) {
        return true;
      } else {
        ENVOY_LOG(warn, "version check failure. file name version: {}, file internal version: {}",
                  plugin_file_info_.version_, func());
      }
    } else {
      ENVOY_LOG(warn, ::dlerror());
    }
  }

  return false;
}

void PluginFile::close() {
  if (handle_) {
    ::dlclose(handle_);
    handle_ = nullptr;
  }
}
} // namespace SrhinoPluginFramework