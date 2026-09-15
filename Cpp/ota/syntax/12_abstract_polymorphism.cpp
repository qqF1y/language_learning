/*************************************************************
 * @FilePath: /Cpp/ota/syntax/12_abstract_polymorphism.cpp
 * @Brief: 语法点 12 —— 抽象基类 / 纯虚函数 / override / 虚析构 / 基类析构里做收尾
 *
 * 真实工程出处：
 *   - src/ota/state_machine/state_machine.hpp
 *       class StateMachineFunction {
 *        public:
 *         StateMachineFunction(StateMachineType type = INIT);
 *         virtual void run() = 0;                    // ← 纯虚，子类必须实现
 *         virtual ~StateMachineFunction();           // ← 虚析构
 *         void setNextStateMachineType(StateMachineType type, int8_t state, const std::string& msg);
 *         static StateMachineData& getStateMachineData();
 *       };
 *   - src/ota/state_machine/idle.hpp
 *       class Idle : public StateMachineFunction {
 *        public:
 *         Idle() : StateMachineFunction(StateMachineType::IDLE) {}
 *         void run() override;                       // ← override
 *         ~Idle() override = default;
 *       };
 *   - state_machine.cpp 的 ~StateMachineFunction()
 *       // 状态机**在基类析构里**执行 process_event，把 "当前状态" 推进到 "下一状态"
 *       //   is_success = state_machine_data_.state_machine.process_event(EventIdle{});
 *       //   if (!is_success) std::terminate();
 *       current_state_machine_type_ = next_state_machine_type_;
 *       writeCurrentStateMachineType();
 *       pub_ota_status->publish(ota_state_msg);
 *
 * 这个设计很特别，务必理解：
 *   子类 run() 只负责"干活 + 声明下一个状态"；
 *   真正的状态推进、落盘、发 topic 都在**基类析构函数**里统一完成。
 *   好处：每个状态类不用重复写这段样板代码，也不可能"忘记推进"。
 ***************************************************************/

#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "mini_log.hpp"

namespace ota::state_machine {

// ===========================================================================
// 1) 状态枚举（简化版，真实工程用 X-Macro）
// ===========================================================================
enum StateMachineType {
  INIT = -1,
  IDLE = 0,
  CHECK_PREREQUISITES = 1,
  DOWNLOAD = 2,
  PARSING_IMG = 3,
  SUCCESS = 6,
  FAILED = 7,
};

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

// ===========================================================================
// 2) 全局共享数据（真实工程是 StateMachineFunction 的 static 成员）
// ===========================================================================
struct StateMachineData {
  StateMachineType state_machine_type = INIT;
  StateMachineType last_state_machine_type = IDLE;
  std::string ota_state_details;
  std::string upgrade_type = "unknown";
  int current_task_index = 0;
  int task_total = 3;
  bool is_receive_task = false;
  // 简化：把"发布 topic"变成打印
  void publish() {
    MINI_INFO("[topic /ota_status] state={} details=\"{}\" progress={}/{}",
              toString(state_machine_type), ota_state_details, current_task_index, task_total);
  }
};

// ===========================================================================
// 3) 抽象基类
// ===========================================================================
class StateMachineFunction {
 public:
  explicit StateMachineFunction(StateMachineType type) : current_state_machine_type_(type) {
    MINI_INFO(">>> 开始执行状态机: {}", toString(type));
  }

  // 纯虚函数：基类无法实现，子类必须实现 -> 本类成为抽象类，不能直接实例化
  virtual void run() = 0;

  // 虚析构：通过基类指针 delete 派生类对象时，保证派生类析构被调用（否则 UB）
  virtual ~StateMachineFunction() {
    MINI_INFO("<<< 结束执行状态机: {} -> {}", toString(current_state_machine_type_),
              toString(next_state_machine_type_));

    // ---- 真实工程在基类析构里做状态推进 / 落盘 / 发消息 ----
    auto& data = getStateMachineData();
    data.last_state_machine_type = data.state_machine_type;
    data.state_machine_type = next_state_machine_type_;
    data.publish();

    // 真实工程还会 writeCurrentStateMachineType() 把状态写盘，用于断点续升级
    MINI_DEBUG("    (真实工程此处写盘: current_ota_state = {})",
               static_cast<int>(next_state_machine_type_));
  }

  static StateMachineData& getStateMachineData() {
    static StateMachineData instance;   // magic static：线程安全且只构造一次
    return instance;
  }

  // 只读访问"我是哪个状态"（非虚，子类不需要重写）
  StateMachineType stateType() const { return current_state_machine_type_; }

 protected:
  // 子类调用它来声明"我干完了，接下来去哪个状态"
  void setNextStateMachineType(StateMachineType type, const std::string& details) {
    next_state_machine_type_ = type;
    state_machine_data_details_ = details;
    getStateMachineData().ota_state_details = details;
  }

  StateMachineType currentState() const { return current_state_machine_type_; }

 private:
  StateMachineType current_state_machine_type_;
  StateMachineType next_state_machine_type_ = INIT;
  std::string state_machine_data_details_;
};

// ===========================================================================
// 4) 各具体状态：override run()
//    注意每个类都极短 —— 样板代码都在基类里
// ===========================================================================
class Idle : public StateMachineFunction {
 public:
  Idle() : StateMachineFunction(StateMachineType::IDLE) {}
  void run() override {
    MINI_INFO("   [Idle] 等待任务……（真实工程用 Waiter 阻塞在这里）");
    auto& data = getStateMachineData();
    if (!data.is_receive_task) {
      MINI_WARN("   [Idle] 还没收到任务，按情况本来应该 wait()；这里直接推进演示");
    }
    setNextStateMachineType(StateMachineType::CHECK_PREREQUISITES, "前置检查");
  }
  ~Idle() override = default;
};

class CheckPrerequisites : public StateMachineFunction {
 public:
  CheckPrerequisites() : StateMachineFunction(StateMachineType::CHECK_PREREQUISITES) {}
  void run() override {
    MINI_INFO("   [CheckPrerequisites] 检查电量、挂起状态、清磁盘……");
    setNextStateMachineType(StateMachineType::DOWNLOAD, "开始下载升级文件");
  }
  ~CheckPrerequisites() override = default;
};

class Download : public StateMachineFunction {
 public:
  Download() : StateMachineFunction(StateMachineType::DOWNLOAD) {}
  void run() override {
    MINI_INFO("   [Download] 下载 + 解压（真实工程走 libcurl）");
    setNextStateMachineType(StateMachineType::PARSING_IMG, "下载完成,开始解析镜像文件");
  }
  ~Download() override = default;
};

class ParsingImg : public StateMachineFunction {
 public:
  ParsingImg() : StateMachineFunction(StateMachineType::PARSING_IMG) {}
  void run() override {
    MINI_INFO("   [ParsingImg] 解析升级包，生成任务列表");
    setNextStateMachineType(StateMachineType::SUCCESS, "解析完成");
  }
  ~ParsingImg() override = default;
};

// final：禁止再被继承（真实工程没有用，但语义正确时推荐加上）
class Success final : public StateMachineFunction {
 public:
  Success() : StateMachineFunction(StateMachineType::SUCCESS) {}
  void run() override {
    auto& data = getStateMachineData();
    ++data.current_task_index;
    MINI_INFO("   [Success] 第 {}/{} 个任务完成", data.current_task_index, data.task_total);
    setNextStateMachineType(StateMachineType::IDLE, "升级成功");
  }
  ~Success() override = default;
};

class Failed : public StateMachineFunction {
 public:
  Failed() : StateMachineFunction(StateMachineType::FAILED) {}
  void run() override {
    MINI_INFO("   [Failed] 播报失败语音，等 /ota_reset");
    setNextStateMachineType(StateMachineType::IDLE, "升级失败");
  }
  ~Failed() override = default;
};

// ===========================================================================
// 5) 陷阱演示类：在构造函数/析构函数里调用虚函数
//    —— 此时虚表还是"当前层"的，调用的是本类版本，不是派生类版本！
// ===========================================================================
class BaseWithVirtualCallInDtor {
 public:
  BaseWithVirtualCallInDtor() {
    MINI_WARN("Base 构造中调用 virtual who(): {}", who());   // 调用 Base::who
  }
  virtual ~BaseWithVirtualCallInDtor() {
    MINI_WARN("Base 析构中调用 virtual who(): {}", who());   // 仍然调用 Base::who！
  }
  virtual std::string who() const { return "Base"; }
};

class Derived : public BaseWithVirtualCallInDtor {
 public:
  std::string who() const override { return "Derived"; }
};

}  // namespace ota::state_machine

int main() {
  using namespace ota::state_machine;
  printTitle("12 抽象基类 / override / 虚析构 / 基类析构收尾");
  printSourceHint("src/ota/state_machine/state_machine.hpp, idle.hpp, state_machine.cpp");

  printStep("1) 抽象类不能实例化");
  // StateMachineFunction f(INIT);   // ❌ 编译错误：abstract class
  MINI_INFO("StateMachineFunction 是抽象类，只能通过派生类使用");

  printStep("2) 每个状态一个对象，run() 多态调用，析构里自动推进状态");
  // 真实工程 ota.cpp 里是这样分发的：
  //   state_machine_list.at(current)();   // 里面 new 一个状态对象并 run()
  {
    auto& data = StateMachineFunction::getStateMachineData();
    data.is_receive_task = true;

    // 用 vector<unique_ptr<Base>> 展示多态：析构时按动态类型调用（虚析构）
    std::vector<std::unique_ptr<StateMachineFunction>> pipeline;
    pipeline.push_back(std::make_unique<Idle>());
    pipeline.push_back(std::make_unique<CheckPrerequisites>());
    pipeline.push_back(std::make_unique<Download>());
    pipeline.push_back(std::make_unique<ParsingImg>());
    pipeline.push_back(std::make_unique<Success>());

    for (auto& state : pipeline) {
      // 注意打印的是"这个对象自己是哪个状态"，而不是全局状态：
      // 全局状态要等对象析构时才推进（见下面的 <<< 输出）
      MINI_INFO("---- 执行 {} ----", toString(state->stateType()));
      state->run();        // ← 动态绑定，调到具体子类的 run()
    }                      // ← 出作用域，基类析构统一"推进状态 + 发 topic"
  }
  MINI_INFO("流水线执行完毕，最终状态 = {}",
            toString(StateMachineFunction::getStateMachineData().state_machine_type));

  printStep("⚠️ 陷阱：创建了状态对象却没有 run()，析构时会用默认 next=INIT 做非法转移");
  // 真实工程的 bug 就是这个形态：
  //   pub_hard_reboot.cpp 原先不调用 setNextStateMachineType，
  //   析构时 next_state_machine_type_ 还是默认的 INIT，
  //   于是执行 "pub_hard_reboot -> init" 这个**转移表里不存在**的边，
  //   process_event 返回 false -> std::terminate() 直接崩进程。
  {
    struct DummyState : StateMachineFunction {
      DummyState() : StateMachineFunction(StateMachineType::IDLE) {}
      void run() override {}   // 故意什么都不做，不设置下一状态
      ~DummyState() override = default;
    };
    DummyState dummy;   // 出作用域析构，观察 next 仍是 INIT
    MINI_WARN("上面那条 <<< 日志里箭头右边是 init，说明发生了非法转移（真实工程会 terminate）");
  }

  printStep("3) 真实工程的分发方式：unordered_map<状态, 工厂函数>");
  // 这里只演示"工厂返回 unique_ptr<Base>"这个关键点
  using StateFactory = std::function<std::unique_ptr<StateMachineFunction>()>;
  const std::unordered_map<StateMachineType, StateFactory> registry = {
      {StateMachineType::IDLE, [] { return std::make_unique<Idle>(); }},
      {StateMachineType::FAILED, [] { return std::make_unique<Failed>(); }},
  };
  {
    auto state = registry.at(StateMachineType::FAILED)();
    state->run();
  }

  printStep("4) 陷阱：构造函数/析构函数里调用虚函数");
  {
    Derived d;   // 构造 Base 时 who() 调的是 Base::who，不是 Derived::who
    MINI_INFO("正常对象上调用 who(): {}", d.who());   // 这里才走 Derived
  }

  printStep("5) 陷阱：对象切片（slicing）");
  {
    // 如果基类不是抽象的，这样按值接收就会"切片"：
    //   void f(Base b) { }   // 传 Derived 进来 -> 派生部分被丢掉，虚函数失效
    // 所以多态参数必须用引用或指针。
    struct Base { virtual std::string name() const { return "Base"; } };
    struct Derived2 : Base { std::string name() const override { return "Derived2"; } };
    auto polymorphic = [](const Base& b) { return b.name(); };   // 传引用 -> 多态生效
    Derived2 d2;
    MINI_INFO("传引用多态生效: {}", polymorphic(d2));
  }

  printStep("小结");
  std::cout << R"(
    - virtual run() = 0;  纯虚函数 -> 基类不能实例化（抽象类）
    - override           : 显式声明"我在重写基类虚函数"，写错签名会编译报错（强烈推荐）
    - final              : 禁止继续继承 / 禁止继续重写
    - 基类必须有 virtual ~  : 否则 delete 基类指针时派生类析构不会被调用（UB）
    - 构造函数/析构函数里调用虚函数：只会调到"当前层"的实现，不是最终派生类！
    - 对象切片：按值传给基类类型会丢失派生部分，多态参数一定要用引用或指针
    - 本工程特色：子类 run() 只管干活 + 声明下一状态，
      真正的状态推进/落盘/发 topic 全在**基类析构函数**里统一完成
)";
  return 0;
}
