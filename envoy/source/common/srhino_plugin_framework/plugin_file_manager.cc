#include "source/common/srhino_plugin_framework/plugin_file_manager.h"

#include "envoy/admin/v3/srhino_plugin_dump.pb.h"
#include "source/common/common/assert.h"
#include "source/common/common/thread.h"
#include <fstream>

namespace SrhinoPluginFramework {
using namespace Envoy;

PluginFileManager::PluginFileManager() {}

PluginFileManager::~PluginFileManager() {}

void PluginFileManager::init(Envoy::Upstream::ClusterManager* cm, Envoy::Server::Admin* admin) {
  ASSERT_IS_MAIN_OR_TEST_THREAD();

  if (!init_) {
    cm_ = cm;
    admin_ = admin;

    config_tracker_entry_ = admin_->getConfigTracker().add(
        "srhino_plugins",
        [this](const Matchers::StringMatcher& /*name_matcher*/) { return dumpSrhinoPlugins(); });

    init_ = true;
  }
}

PluginFileSharedPtr PluginFileManager::addPluginFile(const std::string& file_name) {
  ASSERT_IS_MAIN_OR_TEST_THREAD();

  // 确保不重复添加
  auto file = getPluginFile(file_name);
  if (file) {
    return file;
  }

  file = std::make_shared<PluginFile>(file_name, cm_, admin_);
  plugins_.emplace(file_name, file);

  return file;
}

void PluginFileManager::delPluginFile(const std::string& file_name) {
  ASSERT_IS_MAIN_OR_TEST_THREAD();

  plugins_.erase(file_name);
}

PluginFileSharedPtr PluginFileManager::getPluginFile(const std::string& file_name) {
  ASSERT_IS_MAIN_OR_TEST_THREAD();

  const auto iter = plugins_.find(file_name);
  if (iter != plugins_.end()) {
    return iter->second;
  }

  return nullptr;
}

std::pair<uintptr_t, uintptr_t> getLibraryRangeFromMaps(std::string lib_path_) {
  std::ifstream maps("/proc/self/maps");
  std::string line;

  uintptr_t min_addr = UINTPTR_MAX;
  uintptr_t max_addr = 0;

  while (std::getline(maps, line)) {
    if (line.find(lib_path_) != std::string::npos) {
      std::istringstream iss(line);
      std::string addr_range;
      iss >> addr_range;

      size_t dash_pos = addr_range.find('-');
      uintptr_t start = std::stoul(addr_range.substr(0, dash_pos), nullptr, 16);
      uintptr_t end = std::stoul(addr_range.substr(dash_pos + 1), nullptr, 16);

      min_addr = std::min(min_addr, start);
      max_addr = std::max(max_addr, end);
    }
  }

  return {min_addr, max_addr};
}

void PluginFileManager::merge(const std::set<std::string>& files) {
  ASSERT_IS_MAIN_OR_TEST_THREAD();

  // 删除
  for (auto iter = plugins_.begin(); iter != plugins_.end();) {
    if (files.find(iter->first) == files.end()) {
      iter = plugins_.erase(iter);
    } else {
      ++iter;
    }
  }

  // 增加
  for (auto& file : files) {
    if (file.empty()) {
      ENVOY_LOG(warn, "rtds get a empty srhino_plugins_layer");
      continue;
    }
    addPluginFile(file);
  }
  for (auto iter = plugins_.begin(); iter != plugins_.end(); iter++) {
    auto range = getLibraryRangeFromMaps("/opt/envoy/" + iter->second->pluginFileInfo().path_);
    std::cout << "file: " << iter->second->pluginFileInfo().path_ << " start: " << std::hex << range.first << " - " << range.second << std::endl;
  }

}

Envoy::ProtobufTypes::MessagePtr PluginFileManager::dumpSrhinoPlugins() const {
  ASSERT_IS_MAIN_OR_TEST_THREAD();

  auto dump = std::make_unique<envoy::admin::v3::SrhinoPluginsDump>();

  // installed plugins
  for (auto& plugin_file : plugins_) {
    if (plugin_file.second->isOpen()) {
      auto installed_plugin = dump->mutable_installed_plugins()->add_plugins();
      installed_plugin->set_name(plugin_file.first);

      // 填充controllers
      FILL_CONTROLLERS(installed_plugin, plugin_file.second->pluginFileInfo(),
                       plugin_file.second->manifest());
    }
  }

  // binded plugins
  for (auto& plugin_file : plugins_) {
    auto& binded_list = plugin_file.second->bindedList();
    for (auto& [name, config] : binded_list) {
      auto dump_binded_plugin = dump->mutable_binded_plugins()->add_plugins();
      dump_binded_plugin->set_name(name);
      dump_binded_plugin->set_config_ptr(reinterpret_cast<uint64_t>(config));
    }
  }

  return dump;
}
} // namespace SrhinoPluginFramework