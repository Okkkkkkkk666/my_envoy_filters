#pragma once

#include <string>
#include <unordered_map>

#include "connection_info.h"
#include "data_slices.h"
#include "header_map.h"
#include "route_info.h"
#include "thread_info.h"
#include "timer.h"
#include "utility/http_code.hpp"
#include "virtual_host_info.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
class Context {
public:
  virtual ~Context() {}

public:
  virtual const VirtualHostInfo& virtualHostInfo() const = 0;
  virtual const RouteInfo& routeInfo() const = 0;
  virtual const ConnectionInfo& connectionInfo() const = 0;
  virtual const ThreadInfo& threadInfo() const = 0;
  virtual void setDirectResponse(Utility::HttpCode code, std::unique_ptr<HeaderMap>&& headers,
                                 std::string&& body) = 0;
  virtual std::unordered_map<std::string, std::string>& sharedData() = 0;
  virtual TimerSharedPtr createTimer(std::chrono::milliseconds interval,
                                     std::function<bool(Timer& timer)> func,
                                     bool run_in_main_thread = true) = 0;
  virtual void overrideDestHost(const std::string_view& host) = 0;
  virtual void setWaitForBodyBufferLimit(uint32_t size) = 0;
  virtual uint32_t waitForBodyBufferLimit() const = 0;
};

class HeaderContext : public virtual Context {
public:
  virtual ~HeaderContext() {}

public:
  virtual HeaderMap& headers() = 0;
  virtual const HeaderMap& headers() const = 0;
  virtual void setBreakBody(bool is_break_body) = 0;
  virtual bool hasBody() const = 0;
  virtual void continued() const = 0;
};

class BodyContext : public virtual Context {
public:
  virtual ~BodyContext() {}

public:
  virtual DataSlices& data() = 0;
  virtual bool hasMoreBody() const = 0;
  virtual void continued() const = 0;
};

class TrailerContext : public virtual Context {
public:
  virtual ~TrailerContext() {}

public:
  virtual HeaderMap& headers() = 0;
  virtual const HeaderMap& headers() const = 0;
};
} // namespace v1_0_x
} // namespace SrhinoPluginFramework