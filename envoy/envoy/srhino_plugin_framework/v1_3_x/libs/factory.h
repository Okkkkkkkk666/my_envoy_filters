#pragma once

#include <string>

#include "config/config.h"
#include "database/database.h"
#include "encode/encode.h"
#include "filesystem/filesystem.h"
#include "net/net.h"
#include "regex/regex.h"
#include "thread_local/thread_local.h"
#include "thread_pool/thread_pool.h"

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {
enum class LogLevel {
  trace = 0,
  debug,
  info,
  warn,
  error,
  critical,
  off,
};

using LibsState = void*;

class Factory {
public:
  virtual ~Factory() = default;

  // 不需要绑定插件就可以使用
public:
  virtual Config::ConfigSharedPtr config() const = 0;
  virtual FileSystem::FileSystemSharedPtr filesystem() const = 0;
  virtual Regex::RegexSharedPtr regex() const = 0;
  virtual Encode::EncodeSharedPtr encode() const = 0;
  virtual ThreadPool& getThreadPoolSingleTon() const = 0;

  // 需要插件绑定后才能使用
public:
  virtual Database::DatabaseSharedPtr database(LibsState libs_state) const = 0;
  virtual Net::NetSharedPtr net(LibsState libs_state) const = 0;
  virtual ThreadLocal::ThreadLocalSharedPtr threadLocal(LibsState libs_state) const = 0;

public:
  virtual void log(LogLevel level, const char* file, const char* func, int line,
                   std::function<std::string()> format_cb) const = 0;
};

} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework