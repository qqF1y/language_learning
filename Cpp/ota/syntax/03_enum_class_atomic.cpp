/*************************************************************
 * @FilePath: /Cpp/ota/syntax/03_enum_class_atomic.cpp
 * @Brief: 语法点 03 —— enum vs enum class、底层类型、std::atomic<枚举>
 *
 * 真实工程出处：
 *   - state_machine.hpp: enum StateMachineType { ... }        (无作用域枚举)
 *   - state_machine.hpp: enum class UpgradeType { ... }        (有作用域枚举)
 *   - StateMachineData:  std::atomic<StateMachineType> state_machine_type
 *                        std::atomic<UpgradeType>     upgrade_type
 *   - ota.cpp:           state_machine_type.load() / .store()
 *
 * 关键区别：
 *   enum        -> 枚举名泄漏到外层作用域，可隐式转 int，可和 int 比较
 *   enum class  -> 必须写 UpgradeType::CLOUD，不会隐式转 int（安全）
 *   两者都可以被 std::atomic 包装（std::atomic 对"可平凡复制类型"都支持）
 ***************************************************************/

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "mini_log.hpp"

namespace ota {

// ===========================================================================
// 1) 无作用域枚举（plain enum）
//    - 名字会泄漏：INIT / IDLE 在 namespace ota 里直接可见
//    - 可以隐式转换为 int
//    - 真实工程用它是因为要进 X-Macro 和 switch 分发
// ===========================================================================
enum StateMachineType {
  INIT = -1,
  IDLE = 0,
  DOWNLOAD = 2,
  PARSING_IMG = 3,
  SUCCESS = 6,
  FAILED = 7,
};

// ===========================================================================
// 2) 有作用域枚举（enum class）
//    - 不泄漏名字，必须 UpgradeType::CLOUD
//    - 不隐式转 int，printf/ostream 都不认识它 -> 必须 static_cast
//    - 可以指定底层类型，控制大小（嵌入式/I2C 协议很有用）
// ===========================================================================
enum class UpgradeType : std::int8_t {
  UNKNOWN = -1,
  LOCAL_NX_SERIAL = 0,
  LOCAL_RK_SERIAL = 3,
  LOCAL_ALL = 6,
  CLOUD = 7,
};

// 想要打印 enum class，得自己写转换（真实工程用 X-Macro 自动生成）
const char* toString(UpgradeType t) {
  switch (t) {
    case UpgradeType::UNKNOWN:         return "unknown";
    case UpgradeType::LOCAL_NX_SERIAL: return "local_nx_serial";
    case UpgradeType::LOCAL_RK_SERIAL: return "local_rk_serial";
    case UpgradeType::LOCAL_ALL:       return "local_all";
    case UpgradeType::CLOUD:           return "cloud";
  }
  return "invalid";
}

// 有作用域枚举 -> 字符串，但也可以用 X-Macro 生成（见例程 02）
const char* toString(StateMachineType t) {
  switch (t) {
    case INIT:        return "init";
    case IDLE:        return "idle";
    case DOWNLOAD:    return "download";
    case PARSING_IMG: return "parsing_img";
    case SUCCESS:     return "success";
    case FAILED:      return "failed";
  }
  return "invalid";
}

// ===========================================================================
// 3) 模拟 StateMachineData：枚举 + 原子
// ===========================================================================
class StateMachineData {
 public:
  // std::atomic<enum> 完全合法（枚举是 trivially copyable）
  // 用途：状态机线程写、ROS2 回调线程读，避免数据竞争
  std::atomic<StateMachineType> state_machine_type{INIT};
  std::atomic<UpgradeType> upgrade_type{UpgradeType::UNKNOWN};
  std::atomic<int> current_task_index{0};

  // 原子操作示例
  bool tryAdvanceIsReceiveTask() {
    // compare_exchange_weak：经典的"检查并置位"原子操作（CAS）
    bool expected = false;
    // 第一次失败返回 false，循环是为了处理 spurious failure
    return is_receive_task.compare_exchange_weak(expected, true);
  }

  std::atomic<bool> is_receive_task{false};
};

}  // namespace ota

int main() {
  using namespace ota;
  printTitle("03 enum / enum class / std::atomic<enum>");
  printSourceHint("src/ota/state_machine/state_machine.hpp (StateMachineData)");

  printStep("1) 无作用域枚举：名字泄漏 + 隐式转 int");
  StateMachineType s = IDLE;            // 不用写 StateMachineType::IDLE
  int raw = s;                          // 隐式转换，合法（危险点）
  MINI_INFO("s = {}, 隐式转 int = {}", toString(s), raw);
  MINI_INFO("枚举可以直接和 int 比较: IDLE == 0 -> {}", IDLE == 0);

  printStep("2) 有作用域枚举：必须限定 + 必须显式转换");
  UpgradeType u = UpgradeType::CLOUD;
  // int bad = u;                       // 编译错误：不会隐式转换
  int u_raw = static_cast<int>(u);      // 必须 static_cast
  MINI_INFO("u = {}, static_cast<int> = {}", toString(u), u_raw);
  MINI_INFO("sizeof(UpgradeType) = {} 字节 (指定了 int8_t)", sizeof(UpgradeType));
  MINI_INFO("sizeof(StateMachineType) = {} 字节 (默认按 int)", sizeof(StateMachineType));

  printStep("3) enum class 转字符串的脆弱写法：漏了 case 编译器不报错");
  // 故意传一个未列出的值，观察 switch 走到 default 分支
  auto weird = static_cast<UpgradeType>(99);
  MINI_INFO("static_cast<UpgradeType>(99) -> \"{}\"  (未定义值不会崩，但语义已错)", toString(weird));

  printStep("4) std::atomic<枚举>：跨线程读写状态");
  StateMachineData data;
  MINI_INFO("初始状态     : {}", toString(data.state_machine_type.load()));

  // 模拟状态机线程推进状态，主线程观察
  std::thread worker([&data] {
    for (auto next : {PARSING_IMG, SUCCESS}) {
      data.state_machine_type.store(next);                    // 原子写
      data.current_task_index.fetch_add(1);                   // 原子自增
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
  });

  while (data.current_task_index.load() < 2) {
    MINI_INFO("观察者看到状态: {} (task_index={})",
              toString(data.state_machine_type.load()),
              data.current_task_index.load());
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  worker.join();
  MINI_INFO("最终状态     : {} (task_index={})",
            toString(data.state_machine_type.load()), data.current_task_index.load());

  printStep("5) 用 CAS 做\"只允许一次\"的原子置位（对应 setIsReceiveTask）");
  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};
  for (int i = 0; i < 8; ++i) {
    threads.emplace_back([&data, &success_count] {
      if (data.tryAdvanceIsReceiveTask()) {
        success_count.fetch_add(1);
      }
    });
  }
  for (auto& t : threads) {
    t.join();
  }
  MINI_INFO("8 个线程同时抢，成功置位次数 = {} (必然是 1)", success_count.load());

  printStep("小结");
  std::cout << R"(
    - enum        : 名字泄漏、隐式转 int，方便 switch / X-Macro
    - enum class  : 安全、可指定底层类型（int8_t 省内存、定协议）
    - atomic<enum>: 枚举是 trivially copyable，可以直接放进 std::atomic
    - load/store/fetch_add/CAS：多线程读写状态的标准做法
    - 危险点：static_cast 出来的"非法枚举值"switch 不会报错，要自己校验
        （真实工程用 isDefined() + unordered_set 兜底）
)";
  return 0;
}
