#pragma once

#include "source/common/common/logger.h"
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <string_view>

extern "C" {
#include <nsq.h>
}

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

class NsqClient : Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  NsqClient(const std::string& server_address, const std::vector<int>& server_ports, const std::string& topic,
            const std::string& channel, const std::string& publish_topic, int max_in_flight, int output_buffer_size,
            int output_buffer_timeout_ms, struct ev_loop* loop = nullptr)
      : server_address_(server_address), server_ports_(server_ports), topic_(topic),
        channel_(channel), publish_topic_(publish_topic), max_in_flight_(max_in_flight), output_buffer_size_(output_buffer_size),
        output_buffer_timeout_ms_(output_buffer_timeout_ms),
        create_loop_(loop == nullptr) {}

public:
  // using Callback = std::function<void(const std::string_view&, std::string&)>;
  using Callback = bool (*)(const std::string_view&, std::string&, bool&, void* user_data);
  void registCallback(Callback cb, void* user_data) {
    cb_ = cb;
    user_data_ = user_data;
  }
  void run();
  void waitRunCompleted();
  void notifyRunCompleted();
  void stop();
  using WaitForStop = std::function<void()>;
  WaitForStop stopAsync();
  // void flushPublish() { ::ev_async_send(ev_loop_, &flush_publish_watcher_); }
  int queue(const std::string&& msg);
  int publish(const std::string_view& msg);
  int multiPublish(const std::string& msg);
  bool connected() const { return nsq_connected_; }

private:
  static void onConnect(struct NSQReader* rdr, struct NSQDConnection* conn);
  static void onClose(struct NSQReader* rdr, struct NSQDConnection* conn);
  static void onMessage(struct NSQReader* rdr, struct NSQDConnection* conn, struct NSQMessage* msg,
                        void* ctx);
  static void stopCb(EV_P_ ev_async* watcher, int);
  static void flushPublishCb(EV_P_ ev_timer* watcher, int);
  static void runCompletedCb(EV_P_ ev_timer* timer, int revents);

private:
  // nsq server info
  const std::string server_address_;
  const std::vector<int> server_ports_;
  std::string topic_;
  std::string channel_;
  std::string publish_topic_;
  int max_in_flight_;
  int output_buffer_size_;
  int output_buffer_timeout_ms_;
  std::string auth_secret_;

  // libnsq
  std::vector<struct NSQReader*> readers_;
  struct ev_loop* ev_loop_{nullptr};
  bool create_loop_{false};

  // libnsq callback
  Callback cb_{nullptr};
  void* user_data_{nullptr};

  // thread stat
  bool run_{false};
  std::condition_variable cv_;
  std::mutex cv_mutex_;
  ev_async stop_watcher_;
  uint64_t msg_count_{0};

  std::mutex publish_mutex_;
  bool nsq_connected_{false}; // 是否有可用健康的 NSQD 连接
  int rr_index_{0}; // 轮询索引

  // multi publish
  std::list<std::string> publish_queue_;

  bool run_completed_{false};
  std::condition_variable runed_cv_;
  std::mutex runed_cv_mutex_;
  struct EvTimer : public ev_timer {
    NsqClient* parent;
  };
  EvTimer run_completed_timer_;
  EvTimer publish_timer_;
};
