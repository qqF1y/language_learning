/*************************************************************
 * @FilePath: /Cpp/ota/syntax/15_thread_pool_future.cpp
 * @Brief: 语法点 15 —— 线程池 + packaged_task + future，并行升级多台设备
 *
 * 真实工程出处：
 *   - src/ota/state_machine/send_img_data.cpp
 *       std::vector<std::future<bool>> futures;
 *       for (const auto& device : state_machine_data_.configures.nx_serial.devices) {
 *         futures.push_back(state_machine_data_.thread_pool->enqueue([device]() -> bool {
 *           bool result = utils::serial::upgradeSerial(device.serial_port,
 *                                                      std::to_string(device.baud_rate), ...);
 *           if (!result) { MJR_ERROR("设备 {} 升级失败", device.serial_port); return false; }
 *           return true;
 *         }));
 *       }
 *       bool is_all_success = true;
 *       for (auto& future : futures) {
 *         if (!future.get()) { is_all_success = false; }     // ← 阻塞取结果
 *       }
 *       setNextStateMachineType(is_all_success ? OPEN_TTY : FAILED, ...);
 *
 *   - third_party/thread_pool 是第三方 header-only 线程池
 *   - configures.nx_serial.devices 是 yaml 里配的一串串口设备
 *   - 线程池大小来自配置 thread_pool_size（a2409.yaml 里是 30）
 *
 * 为什么用线程池而不是直接 std::thread？
 *   设备数少时无所谓；设备多/任务频繁时，反复创建线程开销大。
 *   线程池把"任务"和"线程"解耦：任务排队，固定数量的线程循环取任务执行。
 ***************************************************************/

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "mini_log.hpp"

namespace ota::utils {

// ===========================================================================
// 1) 一个够用的线程池
//    核心三件套：任务队列 + 互斥量 + 条件变量
// ===========================================================================
class ThreadPool {
 public:
  explicit ThreadPool(std::size_t thread_count) {
    threads_.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
      threads_.emplace_back([this, i] { workerLoop(i); });
    }
    MINI_INFO("线程池启动，线程数 = {}", thread_count);
  }

  // 析构：置停止标志 -> 唤醒所有线程 -> join
  ~ThreadPool() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopping_ = true;
    }
    cv_.notify_all();       // 关键：不唤醒的话，睡在 cv 上的线程永远不退出，join 会卡死
    for (auto& t : threads_) {
      if (t.joinable()) {
        t.join();
      }
    }
    MINI_INFO("线程池已关闭");
  }

  // 禁止拷贝（线程不可拷贝）
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  // -------------------------------------------------------------------------
  // enqueue：把任意可调用对象丢进队列，返回 std::future<R>
  //   packaged_task 的作用：把"可调用对象"包装成能自动 set_value 的任务，
  //   并从它身上取到配套的 future（get_future 只能调一次）
  // -------------------------------------------------------------------------
  template <typename F, typename... Args>
  auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
    using ReturnType = std::invoke_result_t<F, Args...>;

    // 用 shared_ptr 包住 packaged_task：std::function 要求可拷贝，
    // 而 packaged_task 是 move-only 的
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<ReturnType> result = task->get_future();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stopping_) {
        throw std::runtime_error("enqueue on stopped ThreadPool");
      }
      tasks_.emplace([task] { (*task)(); });   // 队列里只存 void() 的壳
    }
    cv_.notify_one();   // 只叫醒一个线程
    return result;
  }

  std::size_t pendingTasks() {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
  }

 private:
  void workerLoop(int index) {
    while (true) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        // 带谓词的 wait：队列非空 或 要停止 才醒
        cv_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });

        if (stopping_ && tasks_.empty()) {
          return;   // 停止且没活了 -> 线程退出
        }

        task = std::move(tasks_.front());   // 先 move 出来，再解锁
        tasks_.pop();
      }   // ← 这里解锁，避免持锁执行任务
      MINI_DEBUG("[线程{}] 开始执行任务", index);
      task();   // 执行任务（异常由 packaged_task 捕获并塞进 future）
      MINI_DEBUG("[线程{}] 任务执行完毕", index);
    }
  }

  std::vector<std::thread> threads_;
  std::queue<std::function<void()>> tasks_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stopping_ = false;
};

// ===========================================================================
// 2) 模拟 yaml 里的 nx_serial.devices
// ===========================================================================
struct SerialDevice {
  std::string name;
  std::string serial_port;
  int baud_rate = 0;
  int device_id = 0;
  bool should_fail = false;   // 演示失败路径
};

// 模拟 utils::serial::upgradeSerial
bool upgradeSerial(const SerialDevice& device) {
  MINI_INFO("  [{}] 开始升级 {} @ {}bps", device.serial_port, device.name, device.baud_rate);
  std::this_thread::sleep_for(std::chrono::milliseconds(80 + device.device_id * 20));
  if (device.should_fail) {
    MINI_ERROR("  [{}] 升级失败", device.serial_port);
    return false;
  }
  MINI_INFO("  [{}] 升级成功", device.serial_port);
  return true;
}

}  // namespace ota::utils

int main() {
  using namespace ota::utils;
  printTitle("15 线程池 + packaged_task + future 并行升级");
  printSourceHint("src/ota/state_machine/send_img_data.cpp");

  printStep("1) 造一批设备（对应 yaml 里的 devices 列表）");
  const std::vector<SerialDevice> devices = {
      {"nx_mcu", "/dev/ttyTHS1", 921600, 0, false},
      {"nx_motor_a", "/dev/ttyUSB0", 115200, 1, false},
      {"nx_motor_b", "/dev/ttyUSB1", 115200, 2, false},
      {"nx_power", "/dev/ttyUSB2", 57600, 3, false},
      {"nx_broken", "/dev/ttyUSB9", 57600, 4, true},   // 故意失败
  };

  printStep("2) 串行升级（基线，用来对比耗时）");
  {
    const auto t0 = std::chrono::steady_clock::now();
    int ok = 0;
    for (const auto& d : devices) {
      if (upgradeSerial(d)) ++ok;
    }
    const auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - t0).count();
    MINI_INFO("串行耗时 {} ms，成功 {}/{}", cost, ok, devices.size());
  }

  printStep("3) 线程池并行升级（完全对应 send_img_data.cpp）");
  {
    constexpr std::size_t kThreadPoolSize = 30;   // 对应 configures.thread_pool_size
    auto pool = std::make_unique<ThreadPool>(kThreadPoolSize);

    const auto t0 = std::chrono::steady_clock::now();

    std::vector<std::future<bool>> futures;
    futures.reserve(devices.size());
    for (const auto& device : devices) {
      // 注意捕获方式：device 是循环变量，**按值捕获**才对（引用捕获会悬空）
      futures.push_back(pool->enqueue([device]() -> bool { return upgradeSerial(device); }));
    }

    bool is_all_success = true;
    for (auto& future : futures) {
      if (!future.get()) {          // 阻塞直到该任务完成，返回 bool
        is_all_success = false;     // 不提前 return，保证所有任务都被等到
      }
    }

    const auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - t0).count();
    MINI_INFO("并行耗时 {} ms，整体结果 = {}", cost, is_all_success ? "成功" : "失败(有设备失败)");

    // 对应：
    //   setNextStateMachineType(is_all_success ? StateMachineType::OPEN_TTY
    //                                          : StateMachineType::FAILED, ...);
    MINI_INFO("下一状态 = {}", is_all_success ? "OPEN_TTY" : "FAILED");
  }

  printStep("4) future 传递非 bool 结果（比如版本号字符串）");
  {
    ThreadPool pool(4);
    std::vector<std::future<std::string>> version_futures;
    for (int i = 0; i < 4; ++i) {
      version_futures.push_back(pool.enqueue([i]() -> std::string {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        return "mcu_v1.0." + std::to_string(i);   // 模拟查询固件版本
      }));
    }
    for (auto& f : version_futures) {
      MINI_INFO("查询到版本: {}", f.get());
    }
  }

  printStep("5) 异常传播：任务里抛的异常会在 future.get() 处重新抛出");
  {
    ThreadPool pool(2);
    auto bad = pool.enqueue([]() -> int { throw std::runtime_error("固件校验失败"); });
    try {
      (void)bad.get();
    } catch (const std::exception& e) {
      MINI_ERROR("future.get() 重新抛出: {}", e.what());
    }
  }

  printStep("6) 对比 std::async（更简单，但不适合任务量大且要限流的场景）");
  {
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<std::future<bool>> fs;
    for (const auto& d : devices) {
      // std::launch::async：强制新线程（不传参时由实现决定，可能被延迟执行！）
      fs.push_back(std::async(std::launch::async, [d] { return upgradeSerial(d); }));
    }
    int ok = 0;
    for (auto& f : fs) {
      if (f.get()) ++ok;
    }
    const auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - t0).count();
    MINI_INFO("std::async 耗时 {} ms，成功 {}/{}（注意：每个任务都可能新建线程，无法限流）",
              cost, ok, devices.size());
  }

  printStep("小结");
  std::cout << R"(
    - 线程池 = 队列 + mutex + condition_variable + 固定数量的 worker
    - packaged_task 把可调用对象变成"能被 future 观测"的任务；get_future() 只能调一次
    - packaged_task 不可拷贝 -> 用 shared_ptr 包一层才能塞进 std::function/queue
    - 取任务时要"先 move 出队列再解锁"，不要在持锁状态下执行任务
    - 析构顺序：置 stopping -> notify_all -> join（少一步就会卡死）
    - 捕获循环变量务必**按值捕获**（[device]），按引用会全部指向同一个悬空对象
    - 收集结果：遍历 future 调 get()，不要因为第一个失败就提前 return，
      否则后面的 future 析构时会阻塞（std::async 的 future 析构还会等待）
    - 任务的异常会被 packaged_task 捕获，并在 future.get() 处重新抛出
    - std::async 不传 launch 策略时是"实现自定义"，可能变成同步执行，务必显式指定
)";
  return 0;
}
