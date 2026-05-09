#include "source/common/srhino_plugin_framework/v1_1_x/thread_info_impl.h"
#include "source/common/common/empty_string.h"

namespace SrhinoPluginFramework {
namespace v1_1_x {
ThreadInfoImpl::ThreadInfoImpl(Envoy::Http::StreamFilterCallbacks* callbacks)
    : callbacks_(callbacks), thread_index_(-1) {}

/**
 * 获取工作线程总数
 * 注意：
 * 正确应该读取 class OptionsImpl中的 concurrency，但这里读取不到。
 * 当启动envoy不手动设置 --concurrency 选项时，此函数工作正常。
 * 
*/
int ThreadInfoImpl::total_threads() const {
  return std::thread::hardware_concurrency();
}

int ThreadInfoImpl::current_thread_index() const {
  if (thread_index_ != -1)
    return thread_index_;

  const std::string& thread_name = callbacks_->dispatcher().name();
  auto pos = thread_name.find_first_of('_');
  if (pos != std::string::npos) {

    thread_index_ = std::atol(thread_name.data() + pos + 1);
  }
  return thread_index_;
}

} // namespace v1_1_x
} // namespace SrhinoPluginFramework