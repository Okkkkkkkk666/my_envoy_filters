#include "source/common/srhino_plugin_framework/v1_3_x/libs/factory_impl.h"
#include "source/common/srhino_plugin_framework/v1_3_x/libs/config/config_impl.h"
#include "source/common/srhino_plugin_framework/v1_3_x/libs/encode/encode_impl.h"
#include "source/common/srhino_plugin_framework/v1_3_x/libs/filesystem/filesystem_impl.h"
#include "source/common/srhino_plugin_framework/v1_3_x/libs/regex/regex_impl.h"
#include "source/common/srhino_plugin_framework/v1_3_x/libs/net/net_impl.h"
#include "source/common/srhino_plugin_framework/v1_3_x/libs/database/database_impl.h"
#include "source/common/srhino_plugin_framework/v1_3_x/libs/thread_local/thread_local_impl.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
FactoryImpl::FactoryImpl(const PluginFileInfo& plugin_file_info, ManifestConstSharedPtr manifest)
    : plugin_file_info_(plugin_file_info), manifest_(manifest) {}

Config::ConfigSharedPtr FactoryImpl::config() const {
  return std::make_shared<Config::ConfigImpl>();
}

FileSystem::FileSystemSharedPtr FactoryImpl::filesystem() const {
  return std::make_shared<FileSystem::FileSystemImpl>(plugin_file_info_, manifest_);
}

Regex::RegexSharedPtr FactoryImpl::regex() const { return std::make_shared<Regex::RegexImpl>(); }

Encode::EncodeSharedPtr FactoryImpl::encode() const {
  return std::make_shared<Encode::EncodeImpl>();
}

ThreadPool& FactoryImpl::getThreadPoolSingleTon() const {
  return Envoy::ThreadSafeSingleton<ThreadPool>::get();
}

Net::NetSharedPtr FactoryImpl::net(LibsState libs_state) const {
  auto factory_context = convertToFactoryContext(libs_state);
  if (factory_context) {
    return std::make_shared<Net::NetImpl>(*factory_context, plugin_file_info_);
  }

  return nullptr;
}

Database::DatabaseSharedPtr FactoryImpl::database(LibsState libs_state) const {
  auto factory_context = convertToFactoryContext(libs_state);
  if (factory_context) {
    return std::make_shared<Database::DatabaseImpl>(*factory_context);
  }

  return nullptr;
}

ThreadLocal::ThreadLocalSharedPtr FactoryImpl::threadLocal(LibsState libs_state) const {
  auto factory_context = convertToFactoryContext(libs_state);
  if (factory_context) {
    return std::make_shared<ThreadLocal::ThreadLocalImpl>(*factory_context, plugin_file_info_);
  }

  return nullptr;
}

void FactoryImpl::log(LogLevel level, const char* file, const char* func, int line,
                      std::function<std::string()> format_cb) const {
  if (static_cast<spdlog::level::level_enum>(level) >= __log_do_not_use_read_comment().level()) {
    __log_do_not_use_read_comment().log(::spdlog::source_loc{file, line, func},
                                        static_cast<spdlog::level::level_enum>(level), format_cb());
  }
}

} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework