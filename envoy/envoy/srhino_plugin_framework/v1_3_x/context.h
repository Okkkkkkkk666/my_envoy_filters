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
namespace v1_3_x {
class Context {
public:
  virtual ~Context() {}

public:
  virtual const VirtualHostInfo& virtualHostInfo() const = 0;
  virtual const RouteInfo& routeInfo() const = 0;
  virtual const ConnectionInfo& connectionInfo() const = 0;
  virtual ThreadInfo& threadInfo() = 0;
  /**
   * 设置本地回复的内容
   * @param code 响应码
   * @param headers 响应headers
   * @param body 响应消息体
   */
  virtual void setDirectResponse(Utility::HttpCode code, std::unique_ptr<HeaderMap>&& headers,
                                 std::string&& body) = 0;
  /**
   * 设置本地回复的内容
   * 注意： body是左值引用类型，调用者要保证body在被发送出去前是有效的。
   * @param code 响应码
   * @param headers 响应headers
   * @param body 响应消息体
   */
  virtual void setDirectResponse(Utility::HttpCode code, std::unique_ptr<HeaderMap>&& headers,
                                 const std::string& body) = 0;
  virtual std::unordered_map<std::string, std::string>& sharedData() = 0;
  virtual TimerSharedPtr createTimer(std::chrono::milliseconds interval,
                                     std::function<bool(Timer& timer)> func,
                                     bool run_in_main_thread = true) = 0;
  virtual void overrideDestHost(const std::string_view& host) = 0;
  virtual void setWaitForBodyBufferLimit(uint32_t size) = 0;
  virtual uint32_t waitForBodyBufferLimit() const = 0;
  virtual std::unique_ptr<HeaderMap> createHeaderMap() const = 0;
  virtual std::unique_ptr<HeaderMap> createHeaderMap(
      const std::initializer_list<std::pair<std::string_view, std::string_view>>& headers)
      const = 0;
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
} // namespace v1_3_x
} // namespace SrhinoPluginFramework