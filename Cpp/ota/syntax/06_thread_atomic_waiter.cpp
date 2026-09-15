/*************************************************************
 * @FilePath: /Cpp/ota/syntax/06_thread_atomic_waiter.cpp
 * @Brief: 语法点 06 —— 线程 / 原子标志 / 条件变量 Waiter
 *
 * 真实工程出处：
 *   - src/utils/waiter/waiter.hpp|.cpp
 *       class Waiter {
 *         ErrorCode wait(const std::function<bool()>& condition, const std::chrono::milliseconds& timeout);
 *         void notify();
 *        private: std::mutex mutex_; std::condition_variable condition_variable_;
 *       };
 *       ErrorCode Waiter::wait(cond, timeout) {
 *         std::unique_lock<std::mutex> lock(mutex_);
 *         condition_variable_.wait_for(lock, timeout, condition);   // ← 带谓词
 *         return condition() ? OK : TIMEOUT;
 *       }
 *   - src/ota/state_machine/idle.cpp   : 状态机在 wait() 里睡到"有任务"为止
 *   - src/ota/state_machine/failed.cpp : 同理，等 /ota_reset
 *
 * 为什么不用 while(!flag) sleep(100ms) 轮询？
 *   轮询：CPU 空转 + 响应延迟最大 100ms
 *   条件变量：立刻响应 + 不占 CPU
 ***************************************************************/

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "mini_log.hpp"

namespace ota::utils::waiter {

enum class ErrorCode { OK, FAILED, TIMEOUT };

// ===========================================================================
// 1) Waiter：对 condition_variable 的极薄封装
//    注意 wait 接收的是一个**谓词**（返回 bool 的函数），而不是一个值
// ===========================================================================
class Waiter {
 public:
  // 带超时：到点还没满足条件就返回 TIMEOUT
  ErrorCode wait(const std::function<bool()>& condition, const std::chrono::milliseconds& timeout) {
    std::unique_lock<std::mutex> lock(mutex_);

    // wait_for(lock, timeout, pred) 内部等价于：
    //   while (!pred()) {
    //     if (cv.wait_for(lock, timeout) == timeout) return false;
    //   }
    // 好处：既能被 notify 唤醒，也能处理"虚假唤醒"(spurious wakeup)
    condition_variable_.wait_for(lock, timeout, condition);

    // 醒来后再确认一次：可能是超时，而不是条件真的成立了
    return condition() ? ErrorCode::OK : ErrorCode::TIMEOUT;
  }

  // 不带超时：一直等到条件成立
  ErrorCode wait(const std::function<bool()>& condition) {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_variable_.wait(lock, condition);
    return condition() ? ErrorCode::OK : ErrorCode::FAILED;
  }

  void notify() { condition_variable_.notify_all(); }

 private:
  std::mutex mutex_;
  std::condition_variable condition_variable_;
};

}  // namespace ota::utils::waiter

namespace ota {

using utils::waiter::ErrorCode;

// ===========================================================================
// 2) 模拟 StateMachineData：原子标志 + Waiter
// ===========================================================================
class StateMachineData {
 public:
  bool getIsReceiveTask() { return is_receive_task_.load(); }

  // 由 ROS2 服务回调线程调用（真实工程是 srvOtaLocalCmd）
  bool setIsReceiveTask(const std::string& upgrade_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_receive_task_.load()) {
      return false;  // 已有任务在跑，拒绝
    }
    is_receive_task_.store(true);
    upgrade_type_ = upgrade_type;
    idle_task_waiter_.notify();  // ← 唤醒睡在 wait() 里的状态机线程
    MINI_INFO("[回调线程] 接收任务成功: {}", upgrade_type);
    return true;
  }

  void resetIsReceiveTask() {
    std::lock_guard<std::mutex> lock(mutex_);
    is_receive_task_.store(false);
  }

  std::string getUpgradeType() {
    std::lock_guard<std::mutex> lock(mutex_);
    return upgrade_type_;
  }

  // 模拟 rclcpp::ok()
  std::atomic<bool> running{true};

  utils::waiter::Waiter& waiter() { return idle_task_waiter_; }

 private:
  utils::waiter::Waiter idle_task_waiter_;
  std::mutex mutex_;
  std::atomic<bool> is_receive_task_{false};
  std::string upgrade_type_;
};

// ===========================================================================
// 3) 模拟状态机线程：Idle 状态就是"睡到有任务"
// ===========================================================================
void stateMachineThread(StateMachineData& data) {
  MINI_INFO("[状态机线程] 进入 IDLE，开始等待任务……");

  auto condition = [&data]() -> bool {
    if (!data.running.load()) return true;      // 进程要退出了
    if (data.getIsReceiveTask()) return true;   // 收到任务了
    return false;
  };

  while (data.running.load()) {
    const auto ret = data.waiter().wait(condition, std::chrono::seconds(5));
    if (ret == ErrorCode::TIMEOUT) {
      MINI_WARN("[状态机线程] 5 秒超时，继续等（对应 real 工程里的超时分支）");
      continue;
    }
    if (data.getIsReceiveTask()) break;
  }

  if (!data.running.load()) {
    MINI_INFO("[状态机线程] 收到退出信号，线程结束");
    return;
  }

  MINI_INFO("[状态机线程] 被唤醒！升级类型 = {}", data.getUpgradeType());
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  MINI_INFO("[状态机线程] 升级流程模拟执行完毕");
}

}  // namespace ota

int main() {
  using namespace ota;
  printTitle("06 线程 / 原子标志 / 条件变量 Waiter");
  printSourceHint("src/utils/waiter/waiter.hpp|.cpp, src/ota/state_machine/idle.cpp");

  printStep("1) 造一个 StateMachineData，起状态机线程");
  StateMachineData data;
  std::thread sm(stateMachineThread, std::ref(data));

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  MINI_INFO("[主线程] 状态机正在睡……");

  printStep("2) 模拟 ROS2 服务回调：置标志 + notify（立刻响应，不轮询）");
  const auto t0 = std::chrono::steady_clock::now();
  const bool accepted = data.setIsReceiveTask("local_all");
  sm.join();
  const auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
  MINI_INFO("任务被接受: {}, 唤醒+执行耗时 {} ms", accepted, cost);

  printStep("3) 重复提交应被拒绝（is_receive_task 还没清）");
  // 上一个任务跑完但标志没清，这里模拟"任务还在跑时又来一个"
  data.setIsReceiveTask("cloud");
  MINI_INFO("再次提交返回: {} (false 表示拒绝)", data.setIsReceiveTask("cloud"));

  printStep("4) 超时路径：条件永远不成立时 wait 返回 TIMEOUT");
  utils::waiter::Waiter w;
  const auto ret = w.wait([] { return false; }, std::chrono::milliseconds(300));
  MINI_INFO("wait(永不成立, 300ms) 返回: {}",
            ret == utils::waiter::ErrorCode::TIMEOUT ? "TIMEOUT" : "OK");

  printStep("5) 优雅退出：running=false 后 wait 立刻返回");
  data.resetIsReceiveTask();
  StateMachineData data2;
  std::thread sm2(stateMachineThread, std::ref(data2));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  MINI_INFO("[主线程] 请求退出……");
  data2.running.store(false);
  data2.waiter().notify();  // 关键：改了条件必须 notify，否则对方继续睡
  sm2.join();

  printStep("小结");
  std::cout << R"(
    - condition_variable 必须配 mutex 使用；用 unique_lock（wait 内部要解锁/加锁）
    - wait(lock, pred) 这种"带谓词"的写法能正确处理 spurious wakeup，务必用它
    - 改完共享条件后必须 notify_all()/notify_one()，否则等待方永远不醒
    - 退出路径也要 notify：否则 join() 会一直卡住
    - 用 std::atomic 做"快读"标志，用 mutex 保护"复合状态"，二者可以共存
    - 对比轮询 while(!flag) sleep()：条件变量零 CPU 占用 + 微秒级响应
)";
  return 0;
}
