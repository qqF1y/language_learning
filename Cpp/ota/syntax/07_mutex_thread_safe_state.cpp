/*************************************************************
 * @FilePath: /Cpp/ota/syntax/07_mutex_thread_safe_state.cpp
 * @Brief: 语法点 07 —— mutex + lock_guard 封装线程安全状态
 *
 * 真实工程出处：
 *   - state_machine.hpp 的 StateMachineData：
 *       bool setIsReceiveTask(UpgradeType type, const std::string& url, const std::string& ver) {
 *         std::lock_guard<std::mutex> lock(mutex_is_receive_task);   // ← RAII 加锁
 *         if (state_machine_type != StateMachineType::IDLE) return false;
 *         if (is_receive_task) return false;
 *         is_receive_task.store(true);
 *         upgrade_type.store(type);
 *         package_url = url;          // ← std::string 不能是 atomic，必须靠 mutex
 *         ota_version = ver;
 *         idle_task_waiter.notify();
 *         return true;
 *       }
 *       std::string getPackageUrl() {
 *         std::lock_guard<std::mutex> lock(mutex_is_receive_task);
 *         return package_url;         // ← 返回副本，绝不返回引用/指针
 *       }
 *
 * 核心规则：
 *   1. 一个 mutex 保护一组"总是要一起改"的变量（这里：is_receive_task + url + version）
 *   2. 用 lock_guard / scoped_lock，不要手写 lock()/unlock()
 *   3. 读取时也加锁，并**返回值副本**，否则锁一释放数据就悬空
 *   4. 不要在持锁期间调用可能阻塞/回调的函数（会死锁）
 ***************************************************************/

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "mini_log.hpp"

namespace ota {

// ===========================================================================
// 1) 反例：没有保护的共享状态
// ===========================================================================
struct UnsafeCounter {
  long long value = 0;
  // value++ 实际是三条指令：load / add / store
  // 两个线程可能同时读到同一个值，各自 +1 再写回 -> 丢更新
  void add() { ++value; }
};

// ===========================================================================
// 2) 正例 A：只需要计数值 -> 用 std::atomic 最轻量
// ===========================================================================
struct AtomicCounter {
  std::atomic<long long> value{0};
  void add() { value.fetch_add(1, std::memory_order_relaxed); }
};

// ===========================================================================
// 3) 正例 B：多个变量要"原子地"一起改 -> 用 mutex
//    完全对应真实工程的 StateMachineData
// ===========================================================================
class StateMachineData {
 public:
  struct Task {
    std::string upgrade_type;
    std::string package_url;
    std::string ota_version;
  };

  // 写入：一整组变量在同一个临界区里改完
  bool setIsReceiveTask(std::string type, std::string url, std::string version) {
    std::lock_guard<std::mutex> lock(mutex_task_);  // 构造加锁，析构解锁（异常安全）
    if (is_receive_task_) {
      return false;
    }
    is_receive_task_ = true;
    task_.upgrade_type = std::move(type);
    task_.package_url = std::move(url);
    task_.ota_version = std::move(version);
    return true;
  }

  // 读取：返回**副本**，锁释放后调用方持有的是自己的数据
  Task getTask() {
    std::lock_guard<std::mutex> lock(mutex_task_);
    return task_;  // 拷贝一份出去
  }

  // 错误示范：返回引用 —— 锁一释放，另一个线程就能改它，调用方读到脏数据
  // const Task& getTaskUnsafe() { std::lock_guard<std::mutex> lock(mutex_task_); return task_; }

  bool isReceiving() const {
    // 单一 bool 用 atomic 读，不必抢 mutex
    return is_receive_task_.load();
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_task_);
    is_receive_task_ = false;
    task_ = Task{};
  }

 private:
  mutable std::mutex mutex_task_;   // mutable: const 成员函数里也能加锁
  std::atomic<bool> is_receive_task_{false};
  Task task_;
};

// ===========================================================================
// 4) 读写锁：读多写少时性能更好（C++17 std::shared_mutex）
// ===========================================================================
class VersionRegistry {
 public:
  void set(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);  // 写锁：独占
    data_[key] = value;
  }

  std::string get(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);  // 读锁：共享，多个读者可同时进
    const auto it = data_.find(key);
    return it == data_.end() ? "" : it->second;
  }

 private:
  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, std::string> data_;
};

}  // namespace ota

int main() {
  using namespace ota;
  printTitle("07 mutex / lock_guard / 线程安全状态封装");
  printSourceHint("src/ota/state_machine/state_machine.hpp (StateMachineData)");

  constexpr int kThreads = 8;
  constexpr int kIters = 200000;

  printStep("1) 反例：无保护的 ++ 会丢更新");
  {
    UnsafeCounter c;
    std::vector<std::thread> ts;
    for (int i = 0; i < kThreads; ++i) {
      ts.emplace_back([&c] {
        for (int j = 0; j < kIters; ++j) c.add();
      });
    }
    for (auto& t : ts) t.join();
    MINI_WARN("期望 {}，实际 {}（少了 {} 次）", static_cast<long long>(kThreads) * kIters, c.value,
              static_cast<long long>(kThreads) * kIters - c.value);
  }

  printStep("2) 正例：atomic 计数");
  {
    AtomicCounter c;
    std::vector<std::thread> ts;
    for (int i = 0; i < kThreads; ++i) {
      ts.emplace_back([&c] {
        for (int j = 0; j < kIters; ++j) c.add();
      });
    }
    for (auto& t : ts) t.join();
    MINI_INFO("期望 {}，实际 {} (一致)", static_cast<long long>(kThreads) * kIters, c.value.load());
  }

  printStep("3) 正例：mutex 保护一组变量（对应 setIsReceiveTask）");
  {
    StateMachineData data;
    std::atomic<int> accepted{0}, rejected{0};
    std::vector<std::thread> ts;
    for (int i = 0; i < kThreads; ++i) {
      ts.emplace_back([&data, &accepted, &rejected, i] {
        if (data.setIsReceiveTask("local_all", "https://x/img.zip", "v" + std::to_string(i))) {
          accepted.fetch_add(1);
        } else {
          rejected.fetch_add(1);
        }
      });
    }
    for (auto& t : ts) t.join();

    const auto task = data.getTask();
    MINI_INFO("{} 个线程抢，成功 {} / 被拒 {}", kThreads, accepted.load(), rejected.load());
    MINI_INFO("胜出者拿到的任务: type={}, url={}, version={}",
              task.upgrade_type, task.package_url, task.ota_version);
    MINI_INFO("三字段是否来自同一个线程(version 与 url 匹配): {}",
              task.package_url == "https://x/img.zip" && !task.ota_version.empty());
  }

  printStep("4) shared_mutex：读可以并发");
  {
    VersionRegistry reg;
    reg.set("mcu", "1.0.3");
    reg.set("rk3588", "2.1.0");

    std::vector<std::thread> ts;
    for (int i = 0; i < 4; ++i) {
      ts.emplace_back([&reg, i] {
        MINI_INFO("读者{} 读到 mcu={}", i, reg.get("mcu"));
      });
    }
    for (auto& t : ts) t.join();
  }

  printStep("小结");
  std::cout << R"(
    - 一个 mutex 保护一组"必须一起改"的变量；不要给每个变量配一把锁
    - lock_guard 是最轻的 RAII 锁；想手动解锁用 unique_lock；多锁用 scoped_lock
    - getter 必须"加锁 + 返回副本"，绝不能返回内部引用
    - 只读的成员函数里加锁，mutex 要声明为 mutable
    - 单一标量优先考虑 std::atomic（无锁、更快）；复合状态用 mutex
    - 读多写少用 std::shared_mutex + shared_lock
    - 持锁期间不要回调外部代码 / 不要做 IO，避免死锁和长临界区
)";
  return 0;
}
