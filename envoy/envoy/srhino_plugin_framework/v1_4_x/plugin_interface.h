#pragma once

#include <string>
#include <memory>
#include <functional>

#include "plugin_status.h"
#include "context.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
class PluginInterface {
public:
  virtual ~PluginInterface() = default;

public:
  /**
   * 接收到下游请求头时触发
   * @param context 环境上下文
   * @return HeaderStatus 插件HTTP头到达事件响应的返回值。框架根据此返回值决定后续插件的执行行为
   */
  virtual HeaderStatus onRequestHeader(HeaderContext& context) = 0;

  /**
   * 接收到下游请求体时触发
   * @param context 环境上下文
   * @return DataStatus 插件HTTP体到达事件响应的返回值。框架根据此返回值决定后续插件的执行行为
   */
  virtual DataStatus onRequestBody(BodyContext& context) = 0;

  /**
   * 接收到下游请求Trailer时触发
   * @param context 环境上下文
   * @return TrailerStatus
   * 插件HTTP的Trailer头到达事件响应的返回值。框架根据此返回值决定后续插件的执行行为
   */
  virtual TrailerStatus onRequestTrailers(TrailerContext& context) = 0;

  /**
   * 接收到上游响应头时触发
   * @param context 环境上下文
   * @return HeaderStatus 插件HTTP头到达事件响应的返回值。框架根据此返回值决定后续插件的执行行为
   */
  virtual HeaderStatus onResponseHeader(HeaderContext& context) = 0;

  /**
   * 接收到上游响应体时触发
   * @param context 环境上下文
   * @return DataStatus 插件HTTP体到达事件响应的返回值。框架根据此返回值决定后续插件的执行行为
   */
  virtual DataStatus onResponseBody(BodyContext& context) = 0;

  /**
   * 接收到上游响应Trailer时触发
   * @param context 环境上下文
   * @return TrailerStatus
   * 插件HTTP的Trailer头到达事件响应的返回值。框架根据此返回值决定后续插件的执行行为
   */
  virtual TrailerStatus onResponseTrailers(TrailerContext& context) = 0;

  /**
   * 写日志，一个请求以及该请求对应的响应都处理完毕时触发
   * @param msg 用于接收日志内容
   */
  virtual void onCommitLog(std::string& msg) = 0;
};
} // namespace v1_4_x
} // namespace SrhinoPluginFramework