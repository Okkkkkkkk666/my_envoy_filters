#pragma once

#include "envoy/server/factory_context.h"
#include "envoy/srhino_plugin_framework/v1_1_x/libs/factory.h"
#include "source/common/common/logger.h"
#include "source/common/srhino_plugin_framework/v1_1_x/manifest.h"
#include "source/common/srhino_plugin_framework/plugin_file_info.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
namespace Libs {

class FactoryImpl : public Factory, public Envoy::Logger::Loggable<Envoy::Logger::Id::plugin> {
public:
  FactoryImpl(const PluginFileInfo& plugin_file_info, ManifestConstSharedPtr manifest);

public:
  Config::ConfigSharedPtr config() const override;
  FileSystem::FileSystemSharedPtr filesystem() const override;
  Regex::RegexSharedPtr regex() const override;
  Encode::EncodeSharedPtr encode() const override;

public:
  Net::NetSharedPtr net(LibsState libs_state) const override;
  Database::DatabaseSharedPtr database(LibsState libs_state) const override;
  ThreadLocal::ThreadLocalSharedPtr threadLocal(LibsState libs_state) const override;

public:
  void log(LogLevel level, const char* file, const char* func, int line,
           std::function<std::string()> format_cb) const override;

private:
  static Envoy::Server::Configuration::FactoryContext*
  convertToFactoryContext(LibsState libs_state) {
    // 利用RTTI判断是否是合法的指针
    return dynamic_cast<Envoy::Server::Configuration::FactoryContext*>(
        reinterpret_cast<Envoy::Server::Configuration::FactoryContextBase*>(libs_state));
  }

private:
  const PluginFileInfo& plugin_file_info_;
  ManifestConstSharedPtr manifest_;
};

} // namespace Libs
} // namespace v1_1_x
} // namespace SrhinoPluginFramework