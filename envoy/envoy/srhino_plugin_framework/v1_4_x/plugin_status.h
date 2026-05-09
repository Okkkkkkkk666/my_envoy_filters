#pragma once

namespace SrhinoPluginFramework {
namespace v1_4_x {

/**
 * 插件HTTP头到达事件响应的返回值。框架根据此返回值决定后续插件的执行行为。
 */
enum class HeaderStatus {
  // 继续执行插件链中剩余的HTTP头处理例程。
  Continue,

  // 中止执行插件链中剩余的任何处理例程，包括body例程。如果需要继续执行当前插件的body例程，
  // 则可以调用SrhinoPluginFramework::Context::setBreakBody来进行设定。
  // 用于决定丢弃当前数据时使用。
  // 如果请求body为空，则该返回值的行为跟Continue一致。
  Break,

  // 暂停执行插件链中剩余的任何处理例程，并且停止从socket缓冲区中取出数据，这将导致通讯对端
  // 逐渐进入等待TCP窗口的状态。
  //
  // 用于在等待外部处理结果时进行使用，例如在HTTP头例程中发起异步HTTP请求一个第三方服务，在
  // 该服务返回前为了不阻塞当前例程的工作线程，则可以暂停接收当前会话的数据，继而让工作线程
  // 有机会去处理其他会话。
  Pause,

  // 继续执行插件链中剩余的HTTP头处理例程，且附加一段body数据。
  // 需调用SrhinoPluginFramework::Context::setAppendBody来添加数据，如果未手动调用或指定附
  // 加的body数据为空，则该返回值的行为跟Continue一致。
  AppendBody,

  // 中止执行插件链中剩余的任何处理例程。
  //
  // 用于决定丢弃当前数据，且回复一个消息给通讯对端时使用。
  // 回复的内容可以调用SrhinoPluginFramework::Context::setDirectResponse来进行设定。
  DirectResponse,
};

/**
 * 插件HTTP体到达事件响应的返回值。框架根据此返回值决定后续插件的执行行为。
 */
enum class DataStatus {
  // 继续执行插件链中剩余的body处理例程。
  //
  // 如果当前插件的HTTP头处理例程返回Break，但在body处理例程中返回Continue，
  // 则会继续执行插件链中剩余的body处理例程之前先执行剩余的HTTP头处理例程。
  Continue,

  // 中止执行插件链中剩余的任何处理例程。
  Break,

  // 暂停执行插件链中剩余的任何处理例程，并且停止从socket缓冲区中取出数据，这将导致通讯对端
  // 逐渐进入等待TCP窗口的状态。
  //
  // 用于在等待外部处理结果时进行使用，例如在HTTP头例程中发起异步HTTP请求一个第三方服务，在
  // 该服务返回前为了不阻塞当前例程的工作线程，则可以暂停接收当前会话的数据，继而让工作线程
  // 有机会去处理其他会话。
  Pause,

  // 暂停执行插件链中剩余的任何处理例程，直到缓冲完毕所有body数据后，再次执行当前的HTTP体处理
  // 例程。
  //
  // 用于例程需要处理完整的body数据时，进行缓冲数据。
  //
  // 当缓冲的body数据超过缓冲区大小限制（默认100MB，可以使用Context::setWaitForBodyBufferLimit进行设置）
  // 时，当前HTTP体处理例程将不再被执行，随后将继续执行插件链中剩余的HTTP体处理例程。想要继续执行当前的
  // HTTP体处理例程，请返回TryWaitForBody。
  //
  // 当缓冲完毕再次执行当前的HTTP体处理例程时，如果继续返回WaitForBody/TryWaitForBody，将被视为Continue。
  // 另外，如果之前该例程返回过Continue，那么之后也将不再被允许继续返回WaitForBody，如果继续返
  // 回WaitForBody，那此时的行为将跟返回Continue一样。
  WaitForBody,

  // 暂停执行插件链中剩余的任何处理例程，直到缓冲完毕所有body数据或超过缓冲区大小限制（默认100MB，可以使用
  // Context::setWaitForBodyBufferLimit进行设置）时，再次执行当前的HTTP体处理例程。
  //
  // 用于例程需要尽可能的处理完整的body数据时，进行缓冲数据，同时缓冲区不够时也能处理部分数据。缓冲区大小可
  // 以使用Context::setWaitForBodyBufferLimit进行设置，但是当超过缓冲大小限制再次执行当前HTTP处理例程时，
  // 由于TCP流式传输，此次收到的数据可能会大于设定值。可以使用BodyContext::hasMoreBody来判断body数据是否
  // 被缓冲完毕，如果BodyContext::hasMoreBody返回false，则表示缓冲完毕；反之，则表示未缓冲完毕且已经超过
  // 缓冲区大小限制，给与最后一次处理已缓冲的数据的机会，后续再有body数据到达时将不会执行当前例程。
  //
  // 当缓冲完毕或超过缓冲大小限制再次执行当前的HTTP体处理例程时，如果继续返回WaitForBody/TryWaitForBody，
  // 将被视为Continue。另外，如果之前该例程返回过Continue，那么之后也将不再被允许继续返回TryWaitForBody，
  // 如果继续返回TryWaitForBody，那此时的行为将跟返回Continue一样。
  TryWaitForBody,

  // 中止执行插件链中剩余的任何处理例程。
  //
  // 用于决定丢弃当前数据，且回复一个消息给通讯对端时使用。
  // 回复的内容可以调用SrhinoPluginFramework::Context::setDirectResponse来进行设定。
  DirectResponse,
};

/**
 * 插件HTTP的Trailer头到达事件响应的返回值。框架根据此返回值决定后续插件的执行行为。
 */
enum class TrailerStatus {
  // 继续执行插件链中剩余的HTTP的Trailer处理例程。
  Continue,

  // 中止执行插件链中剩余的任何处理例程。
  Break,

  // 中止执行插件链中剩余的任何处理例程。
  // 用于决定丢弃当前数据，且回复一个消息给通讯对端时使用。
  // 回复的内容可以调用SrhinoPluginFramework::Context::setDirectResponse来进行设定。
  DirectResponse
};

} // namespace v1_4_x
} // namespace SrhinoPluginFramework