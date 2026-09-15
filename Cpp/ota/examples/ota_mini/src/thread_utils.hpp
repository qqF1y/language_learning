/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/thread_utils.hpp
 * @Brief: Waiter（条件变量）+ ThreadPool（任务队列 + future）
 *
 * 对应真实工程：
 *   - src/utils/waiter/waiter.hpp|.cpp
 *   - third_party/thread_pool
 *
 * 例程 06 / 15 已分别详解，这里是把它们合起来给 OTA 用。
 ***************************************************************/
#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "log.hpp"
#include "result.hpp"

namespace ota_mini {

// ===========================================================================
// Waiter：状态机用它"睡到有任务为止"，而不是 while(!flag) sleep 轮询
// ===========================================================================
class Waiter {
 public:
  ErrorCode wait(const std::function<bool()>& condition, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait_for(lock, timeout, condition);          // 带谓词：自动处理虚假唤醒
    return condition() ? ErrorCode::OK : ErrorCode::TIMEOUT;
  }

  ErrorCode wait(const std::function<bool()>& condition) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, condition);
    return condition() ? ErrorCode::OK : ErrorCode::FAILED;
  }

  void notify() { cv_.notify_all(); }

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
};

// ===========================================================================
// ThreadPool：并行给多台串口设备发固件
// ===========================================================================
class ThreadPool {
 public:
  explicit ThreadPool(std::size_t thread_count) {
    if (thread_count == 0) {
      thread_count = 1;
    }
    workers_.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
      workers_.emplace_back([this, i] { workerLoop(i); });
    }
    OTA_LOG_INFO("线程池启动，worker 数量 = {}", thread_count);
  }

  ~ThreadPool() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopping_ = true;
    }
    cv_.notify_all();                    // 不唤醒的话 join 会卡死
    for (auto& t : workers_) {
      if (t.joinable()) {
        t.join();
      }
    }
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  template <typename F, typename... Args>
  auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
    using ReturnType = std::invoke_result_t<F, Args...>;

    // packaged_task 是 move-only，std::function 要求可拷贝 -> 用 shared_ptr 包一层
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<ReturnType> future = task->get_future();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stopping_) {
        throw std::runtime_error("enqueue on stopped ThreadPool");
      }
      tasks_.emplace([task] { (*task)(); });
    }
    cv_.notify_one();
    return future;
  }

 private:
  void workerLoop(int index) {
    while (true) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
        if (stopping_ && tasks_.empty()) {
          return;
        }
        task = std::move(tasks_.front());
        tasks_.pop();
      }                                        // 先解锁再执行任务
      OTA_LOG_DEBUG("[worker{}] 执行任务", index);
      task();
    }
  }

  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stopping_ = false;
};

}  // namespace ota_mini
