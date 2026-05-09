#include "source/common/access_log/access_log_manager_impl.h"

#include <string>

#include "envoy/common/exception.h"

#include "source/common/common/assert.h"
#include "source/common/common/fmt.h"
#include "source/common/common/lock_guard.h"
#include "source/common/formatter/substitution_formatter.h"

#include "absl/container/fixed_array.h"

namespace Envoy {
namespace AccessLog {

AccessLogManagerImpl::~AccessLogManagerImpl() {
  for (auto& [log_key, log_file_ptr] : access_logs_) {
    ENVOY_LOG(debug, "destroying access logger {}", log_key);
    log_file_ptr.reset();
  }
  if (nsq_loop_thread_ != nullptr) {
    if (nsq_client_) {
      nsq_client_->stop();
    }
    nsq_loop_thread_->join();
  }
  ENVOY_LOG(debug, "destroyed access loggers");
}

void AccessLogManagerImpl::reopen() {
  for (auto& iter : access_logs_) {
    iter.second->reopen();
  }
}

void AccessLogManagerImpl::nsqLoopThreadFunc() {
  nsq_client_->run();
}

AccessLogFileSharedPtr
AccessLogManagerImpl::createAccessLog(const Filesystem::FilePathAndType& file_info, bool send_to_nsq = false) {
  auto file = api_.fileSystem().createFile(file_info);
  std::string file_name = file->path();
  if (access_logs_.count(file_name)) {
    return access_logs_[file_name];
  }
  access_logs_[file_name] =
      std::make_shared<AccessLogFileImpl>(std::move(file), dispatcher_, send_to_nsq, *nsq_client_, lock_, file_stats_,
                                          file_flush_interval_msec_, api_.threadFactory());
  return access_logs_[file_name];
}

AccessLogFileImpl::AccessLogFileImpl(Filesystem::FilePtr&& file, Event::Dispatcher& dispatcher,
                                     bool send_to_nsq, NsqClient& nsq_client,
                                     Thread::BasicLockable& lock, AccessLogFileStats& stats,
                                     std::chrono::milliseconds flush_interval_msec,
                                     Thread::ThreadFactory& thread_factory)
    : file_(std::move(file)), file_lock_(lock),
      flush_timer_(dispatcher.createTimer([this]() -> void {
        stats_.flushed_by_timer_.inc();
        flush_event_.notifyOne();
        flush_timer_->enableTimer(flush_interval_msec_);
      })),
      thread_factory_(thread_factory), flush_interval_msec_(flush_interval_msec), stats_(stats),
      send_to_nsq_(send_to_nsq), nsq_client_(nsq_client), nsq_flush_buffer_len_(0), nsq_about_to_write_buffer_len_(0) {
  flush_timer_->enableTimer(flush_interval_msec_);
  auto open_result = open();
  if (!open_result.return_value_) {
    throw EnvoyException(fmt::format("unable to open file '{}': {}", file_->path(),
                                     open_result.err_->getErrorDetails()));
  }
}

Filesystem::FlagSet AccessLogFileImpl::defaultFlags() {
  static constexpr Filesystem::FlagSet default_flags{1 << Filesystem::File::Operation::Write |
                                                     1 << Filesystem::File::Operation::Create |
                                                     1 << Filesystem::File::Operation::Append};

  return default_flags;
}

Api::IoCallBoolResult AccessLogFileImpl::open() {
  Api::IoCallBoolResult result = file_->open(defaultFlags());
  return result;
}

void AccessLogFileImpl::reopen() { reopen_file_ = true; }

AccessLogFileImpl::~AccessLogFileImpl() {
  {
    Thread::LockGuard lock(write_lock_);
    flush_thread_exit_ = true;
    flush_event_.notifyOne();
  }
  ENVOY_LOG(trace, "accesslog exit: {}", file_->path());

  if (flush_thread_ != nullptr) {
    flush_thread_->join();
  }

  // Flush any remaining data. If file was not opened for some reason, skip flushing part.
  if (send_to_nsq_ && nsq_client_.connected()) {
    ENVOY_LOG(trace, "flush remaining data to nsq");
    doNsqWrite(nsq_flush_buffer_);
  } else if (file_->isOpen()) {
    if (flush_buffer_.length() > 0) {
      doWrite(flush_buffer_);
    }
    const Api::IoCallBoolResult result = file_->close();
    ASSERT(result.return_value_, fmt::format("unable to close file '{}': {}", file_->path(),
                                             result.err_->getErrorDetails()));
  }
}

void AccessLogFileImpl::doWrite(Buffer::Instance& buffer) {
  Buffer::RawSliceVector slices = buffer.getRawSlices();

  // We must do the actual writes to disk under lock, so that we don't intermix chunks from
  // different AccessLogFileImpl pointing to the same underlying file. This can happen either via
  // hot restart or if calling code opens the same underlying file into a different
  // AccessLogFileImpl in the same process.
  // TODO PERF: Currently, we use a single cross process lock to serialize all disk writes. This
  //            will never block network workers, but does mean that only a single flush thread can
  //            actually flush to disk. In the future it would be nice if we did away with the cross
  //            process lock or had multiple locks.
  {
    Thread::LockGuard lock(file_lock_);
    for (const Buffer::RawSlice& slice : slices) {
      absl::string_view data(static_cast<char*>(slice.mem_), slice.len_);
      const Api::IoCallSizeResult result = file_->write(data);
      if (result.ok() && result.return_value_ == static_cast<ssize_t>(slice.len_)) {
        stats_.write_completed_.inc();
      } else {
        // Probably disk full.
        stats_.write_failed_.inc();
      }
    }
  }

  stats_.write_total_buffered_.sub(buffer.length());
  buffer.drain(buffer.length());
}

void AccessLogFileImpl::doNsqWrite(std::vector<std::string>& buffer) {
  if (buffer.empty()) {
    return; // 如果没有数据，直接返回
  }

  // 构造 NSQ 消息格式
  // NSQ 消息格式: [msg_count][msg_len1][msg1][msg_len2][msg2]...
  // msg_count: 消息数量
  // msg_len: 每条消息的长度
  // msg: 消息内容
  // 注意：NSQ 消息格式需要使用网络字节序（大端序）

  // NSQ MPUB 限制: 每个 MPUB 请求最大 15MB，留有1MB的安全余量
  const uint32_t MAX_BATCH_SIZE = 14 * 1024 * 1024;
  std::vector<std::string> current_batch;
  uint32_t current_batch_bytes = 4, current_buffer_len = 0;
  
  for(auto& msg : buffer) {
    ENVOY_LOG(trace, "msg.size:{}", std::to_string(msg.size()));
    uint32_t current_batch_bytes_next = current_batch_bytes + msg.size() + 4;
    if(current_batch_bytes_next > MAX_BATCH_SIZE) {
      sendBatchToNsq(current_batch, current_buffer_len);
      current_batch.clear();
      current_batch_bytes = 4;
      current_buffer_len = 0;
    }
    current_batch_bytes += 4 + msg.size();
    current_buffer_len += msg.size();
    current_batch.push_back(std::move(msg));
  }

  // 发送最后一批数据
  if(!current_batch.empty()) {
    sendBatchToNsq(current_batch, current_buffer_len);
  }

  buffer.clear();
}

void AccessLogFileImpl::sendBatchToNsq(std::vector<std::string>& buffer, uint32_t buffer_len) {
  if (buffer.empty()) {
    return; // 如果没有数据，直接返回
  }

  ENVOY_LOG(trace, "buffer.size:{}, buffer_len:{}", std::to_string(buffer.size()), std::to_string(buffer_len));

  // 构造 NSQ 消息格式
  // NSQ 消息格式: [msg_count][msg_len1][msg1][msg_len2][msg2]...
  // msg_count: 消息数量
  // msg_len: 每条消息的长度
  // msg: 消息内容
  // 注意：NSQ 消息格式需要使用网络字节序（大端序）
  uint32_t msg_count = buffer.size();
  std::string write_buffer;
  size_t write_buffer_size = 4 + buffer_len + (4 * msg_count);
  write_buffer.reserve(write_buffer_size);
  ENVOY_LOG(trace, "msg_count: {}", msg_count);

  msg_count = htonl(msg_count);
  write_buffer.append(reinterpret_cast<const char*>(&msg_count), sizeof(msg_count));

  for (const std::string& msg : buffer) {
    uint32_t msg_len = htonl(static_cast<uint32_t>(msg.size()));
    write_buffer.append(reinterpret_cast<const char*>(&msg_len), sizeof(msg_len));
    write_buffer.append(msg);
  }

  auto ret = nsq_client_.queue(std::move(write_buffer));
  if (ret == 0) {
    stats_.write_completed_.add(buffer.size());
  } else {
    ENVOY_LOG(debug, "publish failed ret: {}", ret);
    // Probably disk full.
    stats_.write_failed_.inc();
  }
  stats_.write_total_buffered_.sub(buffer_len);
  buffer.clear();
}

void AccessLogFileImpl::flushThreadFunc() {

  while (true) {
    std::unique_lock<Thread::BasicLockable> flush_lock;

    {
      Thread::LockGuard write_lock(write_lock_);

      // flush_event_ can be woken up either by large enough flush_buffer or by timer.
      // In case it was timer, flush_buffer_ can be empty.
      while (flush_buffer_.length() == 0 && nsq_flush_buffer_len_ == 0 && !flush_thread_exit_ && !reopen_file_) {
        // CondVar::wait() does not throw, so it's safe to pass the mutex rather than the guard.
        flush_event_.wait(write_lock_);
      }
      if (flush_thread_exit_) {
        return;
      }

      // 当nsq突然断开，且残余有未flush的数据时，会导致上面的while循环失效。因此，先将数据保存至nsq_about_to_write_buffer
      if (nsq_flush_buffer_len_ > 0) {
        flush_lock = std::unique_lock<Thread::BasicLockable>(flush_lock_);
        nsq_about_to_write_buffer_ = std::move(nsq_flush_buffer_);
        nsq_about_to_write_buffer_len_ = nsq_flush_buffer_len_;
        nsq_flush_buffer_len_ = 0;
        ASSERT(nsq_flush_buffer_.size() == 0);
      } else if (flush_buffer_.length() > 0) {
        flush_lock = std::unique_lock<Thread::BasicLockable>(flush_lock_);
        about_to_write_buffer_.move(flush_buffer_);
        ASSERT(flush_buffer_.length() == 0);
      } else {
        ENVOY_LOG(debug, "reopen_file {}", reopen_file_);
      }
    }

    // if we failed to open file before, then simply ignore
    if (reopen_file_) {
      reopen_file_ = false;
      if (file_->isOpen()) {
        const Api::IoCallBoolResult result = file_->close();
        ASSERT(result.return_value_, fmt::format("unable to close file '{}': {}", file_->path(),
                                                 result.err_->getErrorDetails()));
        const Api::IoCallBoolResult open_result = open();
        if (!open_result.return_value_) {
          stats_.reopen_failed_.inc();
          return;
        }
      }
    }

    if (send_to_nsq_ && nsq_client_.connected()) {
      doNsqWrite(nsq_about_to_write_buffer_);
    }
    if (file_->isOpen() && about_to_write_buffer_.length() > 0) {
      doWrite(about_to_write_buffer_);
    }
  }
}

void AccessLogFileImpl::flush() {
  std::unique_lock<Thread::BasicLockable> flush_buffer_lock;

  {
    Thread::LockGuard write_lock(write_lock_);

    // flush_lock_ must be held while checking this or else it is
    // possible that flushThreadFunc() has already moved data from
    // flush_buffer_ to about_to_write_buffer_, has unlocked write_lock_,
    // but has not yet completed doWrite(). This would allow flush() to
    // return before the pending data has actually been written to disk.
    flush_buffer_lock = std::unique_lock<Thread::BasicLockable>(flush_lock_);

    if (send_to_nsq_ && nsq_client_.connected()) {
      if (nsq_flush_buffer_.size() == 0) {
        return;
      }

      nsq_about_to_write_buffer_ = std::move(nsq_flush_buffer_);
      nsq_about_to_write_buffer_len_ = nsq_flush_buffer_len_;
      nsq_flush_buffer_len_ = 0;
      ASSERT(nsq_flush_buffer_.size() == 0);
    } else {
      if (flush_buffer_.length() == 0) {
        return;
      }

      about_to_write_buffer_.move(flush_buffer_);
      ASSERT(flush_buffer_.length() == 0);
    }
  }

  if (send_to_nsq_ && nsq_client_.connected()) {
    doNsqWrite(nsq_about_to_write_buffer_);
  } else {
    doWrite(about_to_write_buffer_);
  }
}

void AccessLogFileImpl::write(absl::string_view data) {
  if (data.empty()) {
    return;
  }

  Thread::LockGuard lock(write_lock_);

  if (flush_thread_ == nullptr) {
    createFlushStructures();
  }

  if (send_to_nsq_ && nsq_client_.connected()) {
    // 为保证envoy的稳定性，当NSQ写入速度太慢时，丢弃部分日志
    if (nsq_flush_buffer_len_ > MAX_BUFFER_SIZE) {
      stats_.write_failed_.inc();
      ENVOY_LOG(error, "drop log, flush_buffer length: {}", nsq_flush_buffer_len_);
      return;
    }

    bool is_split_package = false;
    if (data.size() > 4) {
      uint32_t length_net;
      memcpy(&length_net, data.data(), sizeof(uint32_t));
      uint32_t length = ntohl(length_net);

      // 检查最高位是否为1 (分包标志)
      if (length & 0x80000000) {
        is_split_package = true;
        const char* start = data.data();
        const char* end = data.data() + data.size();

        while (start < end) {
          if (end - start < 4)
            break; // 异常

          memcpy(&length_net, start, sizeof(uint32_t));
          uint32_t current_len = ntohl(length_net) & 0x7FFFFFFF;
          // 分包总大小 = 24字节头 + 数据长度
          uint32_t packet_size = sizeof(Formatter::PacketHeader) + current_len;
          ENVOY_LOG(trace, "packet_size:{}", std::to_string(packet_size));

          if (start + packet_size > end) {
            // 数据不完整，回退到整体发送或报错
            is_split_package = false;
            break;
          }

          // 将拆分后的独立包加入 Buffer
          stats_.write_buffered_.inc();
          stats_.write_total_buffered_.add(packet_size);
          nsq_flush_buffer_.emplace_back(start, packet_size);
          nsq_flush_buffer_len_ += packet_size;

          start += packet_size;
        }
      }
    }

    // 如果不是分包数据，按原有逻辑处理
    if (!is_split_package) {
      stats_.write_buffered_.inc();
      stats_.write_total_buffered_.add(data.length());
      nsq_flush_buffer_.emplace_back(std::string(data));
      nsq_flush_buffer_len_ += data.length();
    }

    if (nsq_flush_buffer_len_ > MIN_FLUSH_SIZE) {
      flush_event_.notifyOne();
    }
  } else {
    // 为保证envoy的稳定性，当磁盘写入速度太慢时，丢弃部分日志
    if (flush_buffer_.length() > MAX_BUFFER_SIZE) {
      stats_.write_failed_.inc();
      ENVOY_LOG(error, "drop log, flush_buffer length: {}", flush_buffer_.length());
      return;
    }

    stats_.write_buffered_.inc();
    stats_.write_total_buffered_.add(data.length());
    flush_buffer_.add(data.data(), data.size());
    if (flush_buffer_.length() > MIN_FLUSH_SIZE) {
      flush_event_.notifyOne();
    }
  }
}

void AccessLogFileImpl::createFlushStructures() {
  flush_thread_ = thread_factory_.createThread([this]() -> void { flushThreadFunc(); },
                                               Thread::Options{"AccessLogFlush"});
}

} // namespace AccessLog
} // namespace Envoy
