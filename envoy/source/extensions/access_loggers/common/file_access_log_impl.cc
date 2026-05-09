#include "source/extensions/access_loggers/common/file_access_log_impl.h"

namespace Envoy {
namespace Extensions {
namespace AccessLoggers {
namespace File {

FileAccessLog::FileAccessLog(const Filesystem::FilePathAndType& access_log_file_info,
                             AccessLog::FilterPtr&& filter, Formatter::FormatterPtr&& formatter,
                             AccessLog::AccessLogManager& log_manager)
    : ImplBase(std::move(filter)), formatter_(std::move(formatter)) {
  log_file_ = log_manager.createAccessLog(access_log_file_info, formatter_->isSendToNsq());
}

void FileAccessLog::emitLog(const Http::RequestHeaderMap& request_headers,
                            const Http::ResponseHeaderMap& response_headers,
                            const Http::ResponseTrailerMap& response_trailers,
                            const StreamInfo::StreamInfo& stream_info) {
  // 受限模式下不写日志
  if (isRestrictedMode()) {
    return;
  }

  log_file_->write(formatter_->format(request_headers, response_headers, response_trailers,
                                      stream_info, absl::string_view()));
}

} // namespace File
} // namespace AccessLoggers
} // namespace Extensions
} // namespace Envoy
