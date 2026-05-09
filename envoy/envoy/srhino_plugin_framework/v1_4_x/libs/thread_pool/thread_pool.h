
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>

namespace SrhinoPluginFramework {
namespace v1_4_x {
class ThreadPool {
public:
  struct Task {
    uint64_t id;
    std::function<void()> function;
  };

  explicit ThreadPool(size_t thread_count = 0) {
    thread_count_ = std::max(size_t(1), thread_count > 0 ? thread_count
                                                         : std::thread::hardware_concurrency() / 2);
    start(thread_count_);
  }

  ~ThreadPool() { stop(); }

  // 提交任务并返回任务ID和future
  template <class F, class... Args>
  auto enqueue(F&& f, Args&&... args)
      -> std::pair<uint64_t, std::future<typename std::result_of<F(Args...)>::type>> {
    using RetType = typename std::result_of<F(Args...)>::type;

    auto task_package = std::make_shared<std::packaged_task<RetType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<RetType> res = task_package->get_future();

    uint64_t task_id;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (stop_flag_) {
        throw std::runtime_error("enqueue on stopped ThreadPool");
      }

      task_id = next_task_id_++;
      tasks_.push({task_id, [task_package]() { (*task_package)(); }});
    }

    cv_.notify_one();
    return {task_id, std::move(res)};
  }

  // 取消指定ID的任务
  bool cancel_task(uint64_t task_id) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (task_id >= next_task_id_ || task_id == 0) {
      return false;
    }

    if (cancelled_tasks_.find(task_id) != cancelled_tasks_.end()) {
      return false;
    }

    cancelled_tasks_.insert(task_id);
    return true;
  }

private:
  void start(size_t thread_count) {
    for (size_t i = 0; i < thread_count; ++i) {
      workers_.emplace_back([this] {
        while (true) {
          Task task;
          bool should_run = true;

          {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_flag_ || !tasks_.empty(); });
            if (stop_flag_ && tasks_.empty()) {
              cancelled_tasks_.clear();
              break;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
            if (cancelled_tasks_.find(task.id) != cancelled_tasks_.end()) {
              cancelled_tasks_.erase(task.id);
              should_run = false;
            }
          }

          if (should_run) {
            task.function();
          }
        }
      });
      if (workers_.back().native_handle()) {
        const std::string thread_name = "pool_" + std::to_string(i);
        ::pthread_setname_np(workers_.back().native_handle(), thread_name.c_str());
      }
    }
  }

  void stop() noexcept {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      stop_flag_ = true;
    }

    cv_.notify_all();

    for (std::thread& thread : workers_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

private:
  size_t thread_count_;
  std::vector<std::thread> workers_;
  std::condition_variable cv_;
  std::mutex mutex_;
  bool stop_flag_{false};
  uint64_t next_task_id_{1};

  std::queue<Task> tasks_;
  std::unordered_set<uint64_t> cancelled_tasks_;
};
using ThreadPoolPtr = std::shared_ptr<ThreadPool>;

} // namespace v1_4_x
} // namespace SrhinoPluginFramework