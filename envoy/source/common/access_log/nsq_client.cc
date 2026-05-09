#include "nsq_client.h"

#include <json-c/json.h>
#include "auth_secret.h"

// clang-format off
// defined in build/src/data_source/libnsq-prefix/src/libnsq/reader.c
#define DEFAULT_LOOKUPD_INTERVAL     5.
#define DEFAULT_COMMAND_BUF_LEN      1024 * 1024 * 16
#define DEFAULT_COMMAND_BUF_CAPACITY 1024 * 1024 * 16
#define DEFAULT_READ_BUF_LEN         16 * 1024
#define DEFAULT_READ_BUF_CAPACITY    16 * 1024
#define DEFAULT_WRITE_BUF_LEN        16 * 1024
#define DEFAULT_WRITE_BUF_CAPACITY   16 * 1024
// clang-format on

#define PUBLISH_TIMER_INTERVAL 1.0

void NsqClient::run() {
  assert(!run_);
  if (run_ || server_ports_.empty()) {
    ENVOY_LOG(error, "NsqClient run error");
    return;
  }
  msg_count_ = 0;
  run_ = true;
  {
    std::unique_lock<std::mutex> locker(runed_cv_mutex_);
    run_completed_ = false;
  }

  // run
  if (create_loop_) {
    ev_loop_ = ::ev_loop_new(0);
  }
  NSQReaderCfg cfg;
  cfg.lookupd_interval = DEFAULT_LOOKUPD_INTERVAL;
  cfg.command_buf_len = DEFAULT_COMMAND_BUF_LEN * 4;
  cfg.command_buf_capacity = DEFAULT_COMMAND_BUF_CAPACITY * 4;
  cfg.read_buf_len = DEFAULT_READ_BUF_LEN * 1024;
  cfg.read_buf_capacity = DEFAULT_READ_BUF_CAPACITY * 1024 * 16;
  cfg.write_buf_len = DEFAULT_WRITE_BUF_LEN * 1024;
  cfg.write_buf_capacity = DEFAULT_WRITE_BUF_CAPACITY * 1024 * 16;
  auth_secret_ = g_auth_secret->getAuthSecretByEnv("se");
  cfg.auth_secret = auth_secret_.data();
  nsq_identify_cfg_init(&cfg.identify_cfg);
  if (!ssl_init_context(NULL, NULL, NULL)) {
    ENVOY_LOG(warn, "Failed to initialize SSL context, disable tls!");
    cfg.identify_cfg.tls_v1 = false;
  } else {
    cfg.identify_cfg.tls_v1 = false;
    ENVOY_LOG(info, "TLS disabled!");
  }
  cfg.identify_cfg.client_id = "se";
  cfg.identify_cfg.hostname = "se";
  cfg.identify_cfg.output_buffer_size = output_buffer_size_;
  cfg.identify_cfg.output_buffer_timeout_ms = output_buffer_timeout_ms_;

  for (size_t i = 0; i < server_ports_.size(); i++) {
    struct NSQReader* reader_ =
        ::new_nsq_reader(ev_loop_, topic_.c_str(), channel_.c_str(), this, &cfg,
                         NsqClient::onConnect, NsqClient::onClose, NULL);
    reader_->max_in_flight = max_in_flight_;
    int rc = ::nsq_reader_connect_to_nsqd(reader_, server_address_.c_str(), server_ports_[i]);
    if (rc > 0) {
      ENVOY_LOG(info, "NsqClient connect to NSQD success: {} {}, {}: {}, rc: {}", topic_, channel_,
                server_address_, server_ports_[i], rc);
      readers_.push_back(reader_);
    } else {
      ENVOY_LOG(error, "NsqClient connect to NSQD failed: {} {}, {}: {}, rc: {}", topic_, channel_,
                server_address_, server_ports_[i], rc);
      ::free_nsq_reader(reader_);
    }
  }

  ev_async_init(&stop_watcher_, NsqClient::stopCb);
  ::ev_async_start(ev_loop_, &stop_watcher_);

  // 定時
  ev_timer_init(&publish_timer_, NsqClient::flushPublishCb, PUBLISH_TIMER_INTERVAL, PUBLISH_TIMER_INTERVAL);
  publish_timer_.parent = this;
  ::ev_timer_start(ev_loop_, &publish_timer_);
  // ev_async_init(&flush_publish_watcher_, NsqClient::flushPublishCb);
  // flush_publish_watcher_.parent = this;
  // ::ev_async_start(ev_loop_, &flush_publish_watcher_);
  // ev_timer_init(&run_completed_timer_, NsqClient::runCompletedCb, 0, 0);
  // run_completed_timer_.parent = this;
  // ::ev_timer_start(ev_loop_, &run_completed_timer_);

  ENVOY_LOG(trace, "NsqClient runing...");
  ::nsq_run(ev_loop_);
  ENVOY_LOG(trace, "NsqClient exit loop");

  // flush Publish
  // ENVOY_LOG(trace, "flush publish...");
  // multiPublish();
  // ::ev_run(ev_loop_, EVRUN_NOWAIT);
  // ENVOY_LOG(trace, "flush publish completed");

  // clean
  ::ev_async_stop(ev_loop_, &stop_watcher_);
  // ::ev_timer_stop(ev_loop_, &run_completed_timer_);
  for (auto& reader_ : readers_) {
    ::free_nsq_reader(reader_);
  }
  readers_.clear();
  if (create_loop_) {
    ::ev_loop_destroy(ev_loop_);
  }

  // notify stop finish
  // ENVOY_LOG(trace, "NsqClient wait for notify stop");
  std::unique_lock<std::mutex> locker(cv_mutex_);
  cv_.notify_one();
  // ENVOY_LOG(trace, "NsqClient notify stop finish");
}

void NsqClient::waitRunCompleted() {
  std::unique_lock<std::mutex> locker(runed_cv_mutex_);

  runed_cv_.wait(locker, [this]{return run_completed_;});
}

void NsqClient::notifyRunCompleted() {
  std::unique_lock<std::mutex> locker(runed_cv_mutex_);

  runed_cv_.notify_one();
  // ENVOY_LOG(trace, "NsqClient notify run completed");
  run_completed_ = true;
}

void NsqClient::stop() {
  assert(run_);
  if (!run_) {
    return;
  }
  run_ = false;

  // send stop event
  std::unique_lock<std::mutex> locker(cv_mutex_);
  ::ev_async_send(ev_loop_, &stop_watcher_);

  // wait to stop
  cv_.wait(locker);
}

NsqClient::WaitForStop NsqClient::stopAsync() {
  assert(run_);
  if (!run_) {
    return nullptr;
  }

  // send stop event
  std::shared_ptr<std::unique_lock<std::mutex>> locker =
      std::make_shared<std::unique_lock<std::mutex>>(cv_mutex_);
  auto wait_for_stop = std::function<void()>([&, locker] {
    cv_.wait(*locker);
    // ENVOY_LOG(trace, "NsqClient wait stop complete");
    run_ = false;
  });

  ::ev_async_send(ev_loop_, &stop_watcher_);
  // ENVOY_LOG(trace, "NsqClient send stop event");

  return wait_for_stop;
}

static void nsq_buffer_add(struct Buffer* buf, const char* name, const struct NSQCmdParams params[],
                           size_t psize, const char* body, const size_t body_length) {
  char b[64];
  size_t l;

  buffer_add(buf, name, strlen(name));

  if (NULL != params) {
    for (size_t i = 0; i < psize; i++) {
      buffer_add(buf, " ", 1);

      switch (params[i].t) {
      case NSQ_PARAM_TYPE_INT:
        l = sprintf(b, "%d", *(reinterpret_cast<int*>(params[i].v)));
        buffer_add(buf, b, l);
        break;
      case NSQ_PARAM_TYPE_CHAR:
        buffer_add(buf, params[i].v, strlen(reinterpret_cast<char*>(params[i].v)));
        break;
      }
    }
  }
  buffer_add(buf, "\n", 1);

  if (NULL != body) {
    uint32_t vv = htonl(static_cast<uint32_t>(body_length));
    buffer_add(buf, &vv, 4);
    buffer_add(buf, body, body_length);
  }
}

void NsqClient::onConnect(struct NSQReader* rdr, struct NSQDConnection* conn) {
  assert(rdr->ctx);
  NsqClient* parent = reinterpret_cast<NsqClient*>(rdr->ctx);

  ENVOY_LOG(info, "NsqClient on connection to NSQReader: {} {}, port: {}, conn ptr: {}", rdr->topic, rdr->channel, conn->port, reinterpret_cast<void*>(conn));
  // publish data
  std::unique_lock<std::mutex> locker(parent->publish_mutex_);
  parent->nsq_connected_ = true;
}

void NsqClient::onClose(struct NSQReader* rdr, struct NSQDConnection* conn) {
  assert(rdr->ctx);
  NsqClient* parent = reinterpret_cast<NsqClient*>(rdr->ctx);

  ENVOY_LOG(debug, "NsqClient on connection close: {} {}, port: {}, conn ptr: {}", rdr->topic, rdr->channel, conn->port, reinterpret_cast<void*>(conn));
  std::unique_lock<std::mutex> locker(parent->publish_mutex_);
  for (auto& reader : parent->readers_) {
    // 还有健康的连接
    if (reader->conns && reader->conns->bs->state == BS_CONNECTED) {
      ENVOY_LOG(debug, "still connected port: {}, conn ptr: {}", reader->conns->port, reinterpret_cast<void*>(reader->conns));
      return;
    }
  }
  ENVOY_LOG(error, "no health connection");
  parent->nsq_connected_ = false;

}

void NsqClient::onMessage(struct NSQReader* rdr, struct NSQDConnection* conn,
                          struct NSQMessage* msg, void* ctx) {
  assert(ctx);
  NsqClient* parent = reinterpret_cast<NsqClient*>(ctx);
  ++(parent->msg_count_);

  // call specified function
  bool finish = true;
  if (likely(parent->cb_)) {
    std::string publish_data;
    bool cache = false;
    finish = parent->cb_({msg->body, msg->body_length}, publish_data, cache, parent->user_data_);
    if (likely(!publish_data.empty())) {
      parent->publish_queue_.emplace_back(std::move(publish_data));
      if (unlikely(!cache || parent->publish_queue_.size() > 200)) {
        // parent->multiPublish();
      }
    }
  }

  // notify the server that the message has been processed and wanna receive more messages
  if (likely(finish)) {
    ::buffer_reset(conn->command_buf);
    ::nsq_finish(conn->command_buf, msg->id);
    if (unlikely(parent->msg_count_ % rdr->max_in_flight == 0)) {
      ::nsq_ready(conn->command_buf, rdr->max_in_flight);
    }
    ::buffered_socket_write_buffer(conn->bs, conn->command_buf);
  }

  // clean
  // don't call free_nsq_message, because the libnsq was be patched: msg is in stack now
  // ::free_nsq_message(msg);
}

void NsqClient::stopCb(EV_P_ ev_async* , int) {
  ENVOY_LOG(trace, "NsqClient stop event callback has be called");

  // break loop
  ::ev_unloop(EV_A_ EVBREAK_ALL);
}

void NsqClient::flushPublishCb(EV_P_ ev_timer* timer, int) {
  ENVOY_LOG(trace, "NsqClient flushPublishCb has be called");
  EvTimer* t = reinterpret_cast<EvTimer*>(timer);
  std::unique_lock<std::mutex> locker(t->parent->publish_mutex_);
  for (const auto& msg : t->parent->publish_queue_) {
    t->parent->multiPublish(msg);
  }
  t->parent->publish_queue_.clear();
}

void NsqClient::runCompletedCb(EV_P_ ev_timer* timer, int) {
  // ENVOY_LOG(trace, "NsqClient run completed");
  EvTimer* t = reinterpret_cast<EvTimer*>(timer);
  t->parent->notifyRunCompleted();
}

int NsqClient::publish(const std::string_view& msg) {
  static constexpr char command_name[] = "PUB";

  if (unlikely(msg.length() > DEFAULT_COMMAND_BUF_CAPACITY)) {
    return -1;
  }

  std::unique_lock<std::mutex> locker(publish_mutex_);
  struct NSQDConnection* conn_ = readers_.empty() ? nullptr : readers_[0]->conns;
  if (unlikely(conn_ == nullptr)) {
    return -1;
  }

  // publish message
  ::buffer_reset(conn_->command_buf);
  const struct NSQCmdParams params[1] = {
      {const_cast<char*>(publish_topic_.c_str()), NSQ_PARAM_TYPE_CHAR},
  };

  // FIXME: if the length of msg greater than capacity of command_buf, the memory will corruputed
  // the current implementation is discard the message, but this solution will lose the message
  ::nsq_buffer_add(conn_->command_buf, command_name, params, 1, msg.data(), msg.length());
  return ::buffered_socket_write_buffer(conn_->bs, conn_->command_buf);
}

int NsqClient::queue(const std::string&& msg) {
  std::unique_lock<std::mutex> locker(publish_mutex_);
  publish_queue_.emplace_back(msg);
  return 0;
}

int NsqClient::multiPublish(const std::string& msg) {
  static constexpr char command_name[] = "MPUB";

  if (unlikely(msg.length() > DEFAULT_COMMAND_BUF_CAPACITY)) {
    return -1;
  }

  struct NSQDConnection* conn_ = NULL;
  for (size_t i = 0; i < readers_.size(); i++) {
    rr_index_ = (rr_index_ + 1) % readers_.size();
    if (readers_[rr_index_]->conns && readers_[rr_index_]->conns->bs &&
        readers_[rr_index_]->conns->bs->state == BS_CONNECTED) {
      conn_ = readers_[rr_index_]->conns;
      break;
    }
  }
  if (unlikely(conn_ == nullptr)) {
    nsq_connected_ = false;
    return -1;
  }

  ::buffer_reset(conn_->command_buf);
  const struct NSQCmdParams params[1] = {
      {const_cast<char*>(publish_topic_.c_str()), NSQ_PARAM_TYPE_CHAR},
  };

  // FIXME: if the length of buffer greater than capacity of command_buf, the memory will corruputed
  // the current implementation is discard the message, but this solution will lose the message
  ::nsq_buffer_add(conn_->command_buf, command_name, params, 1, msg.c_str(), msg.length());
  ::buffered_socket_write_buffer(conn_->bs, conn_->command_buf);
  ENVOY_LOG(trace, "multiPublish success through rr_index: {}. length: {}", rr_index_, msg.length());

  return 0;
}