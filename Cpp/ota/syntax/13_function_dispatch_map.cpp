/*************************************************************
 * @FilePath: /Cpp/ota/syntax/13_function_dispatch_map.cpp
 * @Brief: 语法点 13 —— 用 unordered_map<枚举, std::function<...>> 做状态分发
 *
 * 真实工程出处（src/ota/ota.cpp，逐行对照）：
 *
 *   void OtaNode::execute_state_machine() {
 *     const std::unordered_map<state_machine::StateMachineType, std::function<void()>> state_machine_list = {
 *         {StateMachineType::INIT,    [&]() { state_machine::Init init; init.run(); }},
 *         {StateMachineType::IDLE,    [&]() { state_machine::Idle idle; idle.run(); }},
 *         {StateMachineType::DOWNLOAD,[&]() { state_machine::Download download; download.run(); }},
 *         ...
 *     };
 *     while (rclcpp::ok() && is_running_) {
 *       auto current = StateMachineFunction::getStateMachineData().state_machine_type.load();
 *       if (state_machine_list.find(current) != state_machine_list.end()) {
 *         try {
 *           state_machine_list.at(current)();          // ← 取出函数并调用
 *         } catch (const std::exception& e) {
 *           MJR_ERROR("current state: {}, error: {}", toString(current), e.what());
 *           std::terminate();                          // ← 异常直接终止进程
 *         }
 *       } else {
 *         MJR_ERROR("invalid state machine type: {}", static_cast<int>(current));
 *         std::terminate();
 *       }
 *     }
 *   }
 *
 * 设计要点：
 *   1. 用 map 代替超长 switch，新增状态只加一行，不改循环结构（开闭原则）
 *   2. lambda 按引用捕获 [&]，能访问循环外面的局部变量
 *   3. 先 find 再 at()：避免 at() 抛 out_of_range
 *   4. 整个调用包在 try/catch 里：**任何**状态异常都走同一个崩溃路径，
 *      保证不会出现"状态机半死不活"的情况
 ***************************************************************/

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "mini_log.hpp"

namespace ota {

enum StateMachineType { INIT = -1, IDLE = 0, CHECK_PREREQUISITES = 1, DOWNLOAD = 2, PARSING_IMG = 3, SUCCESS = 6, FAILED = 7 };

const char* toString(StateMachineType t) {
  switch (t) {
    case INIT: return "init";
    case IDLE: return "idle";
    case CHECK_PREREQUISITES: return "check_prerequisites";
    case DOWNLOAD: return "download";
    case PARSING_IMG: return "parsing_img";
    case SUCCESS: return "success";
    case FAILED: return "failed";
  }
  return "invalid";
}

struct StateMachineData {
  StateMachineType state_machine_type = INIT;
  int battery_percentage = 100;
};

StateMachineData& data() {
  static StateMachineData d;
  return d;
}

// ===========================================================================
// 各种状态实现（这里是普通函数；真实工程是继承 StateMachineFunction 的类）
// ===========================================================================
void doInit() {
  MINI_INFO("  [init] 读机型、恢复上次中断状态");
  data().state_machine_type = IDLE;
}

void doIdle() {
  MINI_INFO("  [idle] 等任务（此处演示：未收到任务就停在 idle）");
  // 真实工程用 Waiter 阻塞；这里为了让演示跑完，直接推进
  data().state_machine_type = CHECK_PREREQUISITES;
}

void doCheckPrerequisites() {
  MINI_INFO("  [check_prerequisites] 电量 = {}%", data().battery_percentage);
  if (data().battery_percentage < 30) {
    MINI_WARN("  电量不足，转 FAILED");
    data().state_machine_type = FAILED;
    return;
  }
  data().state_machine_type = DOWNLOAD;
}

void doDownload() {
  MINI_INFO("  [download] 下载升级包");
  data().state_machine_type = PARSING_IMG;
}

void doParsingImg() {
  MINI_INFO("  [parsing_img] 解析镜像");
  data().state_machine_type = SUCCESS;
}

void doSuccess() {
  MINI_INFO("  [success] 全部完成");
  data().state_machine_type = IDLE;
}

void doFailed() {
  MINI_ERROR("  [failed] 升级失败，等待 /ota_reset");
  data().state_machine_type = IDLE;
}

// 故意抛异常的状态，用来演示 try/catch
void doExplode() {
  throw std::runtime_error("模拟状态内部异常（比如磁盘满）");
}

}  // namespace ota

int main() {
  using namespace ota;
  printTitle("13 unordered_map<Enum, function<void()>> 状态分发");
  printSourceHint("src/ota/ota.cpp  OtaNode::execute_state_machine()");

  printStep("1) 构建分发表（对应真实工程的 state_machine_list）");
  // lambda 按引用捕获 [&]，可以访问外面的 stop_flag / step_count
  bool is_running = true;
  int step_count = 0;
  constexpr int kMaxSteps = 20;

  const std::unordered_map<StateMachineType, std::function<void()>> state_machine_list = {
      {StateMachineType::INIT, [&]() { doInit(); }},
      {StateMachineType::IDLE, [&]() { doIdle(); }},
      {StateMachineType::CHECK_PREREQUISITES, [&]() { doCheckPrerequisites(); }},
      {StateMachineType::DOWNLOAD, [&]() { doDownload(); }},
      {StateMachineType::PARSING_IMG, [&]() { doParsingImg(); }},
      {StateMachineType::SUCCESS, [&]() { doSuccess(); }},
      {StateMachineType::FAILED, [&]() { doFailed(); }},
      // 演示：把非法状态映射到会抛异常的处理函数
      {static_cast<StateMachineType>(999), [&]() { doExplode(); }},
  };
  MINI_INFO("分发表里有 {} 个条目", state_machine_list.size());

  printStep("2) 主循环：load -> find -> at()() —— 与真实工程结构一致");
  while (is_running && ++step_count <= kMaxSteps) {
    const auto current = data().state_machine_type;

    if (state_machine_list.find(current) != state_machine_list.end()) {
      try {
        state_machine_list.at(current)();   // 取出 std::function 并调用
      } catch (const std::exception& e) {
        // 真实工程这里直接 std::terminate()：状态机出错绝不静默继续
        MINI_ERROR("current state: {}, state machine error: {}", toString(current), e.what());
        MINI_WARN("  (真实工程此处 std::terminate()；演示里改为跳出循环)");
        is_running = false;
      }
    } else {
      MINI_ERROR("invalid state machine type: {}", static_cast<int>(current));
      is_running = false;
    }

    // 真实工程没有这个退出条件，靠的是状态自己一直转或者进程终止；
    // 这里为了让演示结束，回到 IDLE 就跳出（第 1 步刚起步就离开 INIT 不算）
    if (data().state_machine_type == StateMachineType::IDLE && step_count > 1) {
      MINI_INFO("回到 IDLE，演示结束");
      break;
    }
  }
  MINI_INFO("主循环退出，共执行 {} 步", step_count);

  printStep("3) find 与 at 的区别：为什么两个都要用");
  {
    const StateMachineType bad = static_cast<StateMachineType>(123);
    // 先 find 判断存在性，就不会踩到 at() 抛的 out_of_range
    MINI_INFO("find(123) == end() ? {}", state_machine_list.find(bad) == state_machine_list.end());
    try {
      state_machine_list.at(bad);   // 不先判断就直接 at() -> 抛异常
    } catch (const std::out_of_range& e) {
      MINI_WARN("at() 直接抛异常: {}", e.what());
    }
    // 另一种写法：用 operator[] 会**默认构造**一个空 function，调用它抛 bad_function_call
    // 真实工程从不这么写，因为空 function 的错误很难定位
  }

  printStep("4) 触发异常路径：把当前状态设成 999");
  {
    data().battery_percentage = 100;
    data().state_machine_type = static_cast<StateMachineType>(999);
    const auto current = data().state_machine_type;
    try {
      state_machine_list.at(current)();
    } catch (const std::exception& e) {
      MINI_ERROR("捕获: state={}, error={}", toString(current), e.what());
    }
  }

  printStep("5) 低电量路径：走 FAILED 分支");
  {
    data().battery_percentage = 10;
    data().state_machine_type = StateMachineType::CHECK_PREREQUISITES;
    state_machine_list.at(data().state_machine_type)();
    state_machine_list.at(data().state_machine_type)();   // FAILED 处理
    MINI_INFO("最终状态 = {}", toString(data().state_machine_type));
  }

  printStep("6) 对比：switch 写法 vs map 写法");
  std::cout << R"(
    switch 写法:
      void dispatch(State s) {
        switch (s) {
          case INIT: doInit(); break;
          case IDLE: doIdle(); break;
          ...
          default: std::terminate();
        }
      }
      - 优点：编译器能做跳转表优化，最快
      - 缺点：新增状态要改 switch；漏写 case 只有 -Wswitch 才提示

    map 写法（本工程采用）:
      - 优点：注册表集中在一处；方便替换/注入 mock（单元测试友好）
      - 缺点：哈希查找 + std::function 间接调用，比 switch 慢一点
               （但状态机一秒钟才跑几次，性能完全无所谓）
)";

  printStep("小结");
  std::cout << R"(
    - std::unordered_map<Enum, std::function<void()>> 是"开闭原则"的廉价实现
    - 用 lambda 包一层是为了统一签名（各个状态的类/函数原型本来不一样）
    - [&] 捕获要小心：分发表的生命周期不能超过被捕获变量
    - 先 find 再 at()；不要把 operator[] 用在这里（会插入空 function）
    - 调用点必须 try/catch：状态机异常要"快速失败"，不能带病继续跑
    - 每个状态对象都是"用完即毁"（局部变量），靠基类析构推进状态
)";
  return 0;
}
