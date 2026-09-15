/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/state_machine.hpp
 * @Brief: 状态机框架 —— 综合例程的核心
 *
 * 综合运用了前面所有语法点：
 *   - X-Macro 生成状态枚举 / toString / stringTo / isDefined   (例程 02)
 *   - enum class + std::atomic<enum>                            (例程 03)
 *   - 抽象基类 + 纯虚函数 + 虚析构 + 基类析构里做状态推进        (例程 12)
 *   - mutex + atomic 的线程安全上下文                            (例程 07)
 *   - Waiter（条件变量）等待任务                                 (例程 06)
 *   - Expected<...> 错误处理                                     (例程 08)
 *   - unordered_map<枚举, function<...>> 分发表                  (例程 13)
 *   - 线程池并行升级                                            (例程 15)
 *
 * 与真实工程 humanoid_ota 的对应关系：
 *   StateMachineType      <-> src/ota/state_machine/state_machine.hpp  OTA_STATE_MACHINE_LIST
 *   TaskType              <-> src/ota/task/task.hpp                    OTA_TASK_TYPE_LIST
 *   StateMachineData      <-> struct StateMachineData
 *   StateMachineFunction  <-> class StateMachineFunction（含"析构里推进"的设计）
 *   kTransitions          <-> boost::sml 的 make_transition_table
 ***************************************************************/
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "config.hpp"
#include "result.hpp"
#include "thread_utils.hpp"

namespace ota_mini::state_machine {

// ===========================================================================
// 1) 状态列表（X-Macro 单一数据源）
//    格式：X(枚举名, 英文名, 中文名, 数值)
// ===========================================================================
#define OTA_STATE_LIST                                          \
  X(INIT, "init", "初始化", -1)                                 \
  X(IDLE, "idle", "空闲状态", 0)                                \
  X(CHECK_PREREQUISITES, "check_prerequisites", "检查前置条件", 1) \
  X(DOWNLOAD, "download", "下载升级包", 2)                      \
  X(PARSING_IMG, "parsing_img", "解析镜像", 3)                  \
  X(CHECK_OTA_TASK, "check_ota_task", "检查升级任务", 4)          \
  X(SEND_IMG_DATA, "send_img_data", "发送固件数据", 5)            \
  X(NX_SELF, "nx_self", "NX 自升级", 6)                         \
  X(RK_SEND_OTA_CMD, "rk_send_ota_cmd", "RK 下发升级命令", 7)     \
  X(SUCCESS, "success", "升级成功", 8)                          \
  X(FAILED, "failed", "升级失败", 9)                            \
  X(REBOOT, "reboot", "重启", 10)                               \
  X(PUB_HARD_REBOOT, "pub_hard_reboot", "发布硬重启", 11)

#define X(name, desc, desc_cn, value) name = value,

enum StateMachineType {
  OTA_STATE_LIST
};
#undef X

const std::string& toString(StateMachineType type, bool is_cn = false);
StateMachineType stringTo(const std::string& type);
bool isDefined(StateMachineType type);

// ===========================================================================
// 2) 对外发布的 OTA 状态码（对应 common_msgs/msg/OtaState）
// ===========================================================================
namespace ota_state {
constexpr int8_t IDLE = 0;
constexpr int8_t PREPARE = 1;
constexpr int8_t DOWNLOAD_START = 2;
constexpr int8_t DOWNLOAD_SUCCESS = 3;
constexpr int8_t UPDATING = 4;
constexpr int8_t WAIT_RESULT = 5;
constexpr int8_t SUCCEED = 6;
constexpr int8_t FAIL = 7;
}  // namespace ota_state

// 对应 common_msgs::msg::OtaState（这里用普通结构体替代 ROS2 消息）
struct OtaStateMsg {
  int8_t state = ota_state::IDLE;
  std::string state_details;
  int download_progress = 0;
  int pkg_current = 0;
  int pkg_total = 0;
};

// ===========================================================================
// 3) 升级任务类型（X-Macro）
//
// ⚠️ 命名空间污染：enum 是**无作用域**的，所有枚举名都会泄漏到
//    ota_mini::state_machine 里。所以 NX_SELF 会和 StateMachineType::NX_SELF
//    撞名（编译报错: 'NX_SELF' conflicts with a previous declaration）。
//    两种解法：
//      ① 真实工程的做法：把 TaskType 放进子命名空间 ota::task
//      ② 本例程的做法：给容易撞名的项加 TASK_ 前缀
//    更根本的解法是改用 enum class，但那样就不能直接 switch 数字了。
// ===========================================================================
#define OTA_TASK_TYPE_LIST                                          \
  X(UNKNOWN, "unknown", "未知", 0)                                  \
  X(NX_SERIAL, "nx_serial", "NX 串口设备", 1)                        \
  X(RK_ETHERCAT, "rk_ethercat", "RK EtherCAT 电机", 2)               \
  X(RK_SELF, "rk_self", "RK 自升级", 3)                             \
  X(TASK_NX_SELF, "nx_self", "NX 自升级", 4)                        \
  X(RK_SERIAL, "rk_serial", "RK 串口设备", 5)

#define X(name, desc, desc_cn, value) name = value,
enum TaskType {
  OTA_TASK_TYPE_LIST
};
#undef X

const std::string& toString(TaskType type, bool is_cn = false);
TaskType stringToTaskType(const std::string& type);
bool isDefined(TaskType type);

// ===========================================================================
// 4) 升级入口类型（enum class，对应 UpgradeType）
// ===========================================================================
enum class UpgradeType : int8_t {
  UNKNOWN = -1,
  LOCAL_ALL = 0,
  LOCAL_RK_MOTOR = 1,
  LOCAL_RK_MCU = 2,
  LOCAL_RK_SELF = 3,
  LOCAL_NX_SELF = 4,
  CLOUD = 5,
};

const std::string& toString(UpgradeType type);

// ===========================================================================
// 5) 路径工具（对应真实工程 src/utils/ros2/ros2.hpp 里那一堆 getXxxPath）
//    全部基于 configures.work_dir 推导
// ===========================================================================
struct Paths {
  std::string work_dir = "/tmp/ota_mini/work";

  std::filesystem::path workDir() const { return work_dir; }
  // 断点续升级用的状态文件（对应 current_ota_state 等）
  std::filesystem::path stateFile() const { return workDir() / "current_ota_state"; }
  std::filesystem::path upgradeTypeFile() const { return workDir() / "current_ota_upgrade_type"; }
  std::filesystem::path packageUrlFile() const { return workDir() / "current_ota_package_url"; }
  std::filesystem::path versionFile() const { return workDir() / "current_ota_version"; }
  std::filesystem::path taskListFile() const { return workDir() / "current_ota_task_list"; }
  std::filesystem::path taskIndexFile() const { return workDir() / "current_ota_task_index"; }
  // 升级包相关
  std::filesystem::path downloadDir() const { return workDir() / "download"; }
  std::filesystem::path packageFile() const { return downloadDir() / "img.zip"; }
  std::filesystem::path imgDir() const { return downloadDir() / "img"; }
  std::filesystem::path versionJsonFile() const { return workDir() / "mcu_version.json"; }
};

// ===========================================================================
// 6) 全局共享上下文（对应 StateMachineData）
// ===========================================================================
struct StateMachineData {
  // ---- 配置 ----
  config::Config configures;

  // ---- 状态机当前状态 ----
  std::atomic<StateMachineType> state_machine_type{StateMachineType::INIT};
  StateMachineType last_state_machine_type = StateMachineType::IDLE;

  // ---- 待发布的消息（对应 /ota_status topic）----
  OtaStateMsg ota_state_msg;
  std::function<void(const OtaStateMsg&)> publish_status;   // 由 OtaNode 注入
  std::function<void(const std::string&)> speak;            // 对应 TTS 客户端

  // ---- 进程运行标志（对应 rclcpp::ok()）----
  std::atomic_bool running{true};

  // ---- 电池 / 挂起 ----
  std::atomic<int> battery_percentage{80};
  std::atomic_bool is_hang_up{false};

  // ---- 任务接收 ----
  bool getIsReceiveTask() { return is_receive_task_.load(); }

  bool setIsReceiveTask(UpgradeType upgrade_type, const std::string& package_url = {},
                        const std::string& ota_version = {}) {
    std::lock_guard<std::mutex> lock(mutex_receive_task_);   // 一组变量一起改，必须同一把锁
    if (state_machine_type.load() != StateMachineType::IDLE) {
      OTA_LOG_WARN("拒绝任务：当前状态 {} 不是 IDLE", toString(state_machine_type.load()));
      return false;
    }
    if (is_receive_task_.load()) {
      OTA_LOG_WARN("拒绝任务：已有任务在处理中");
      return false;
    }
    is_receive_task_.store(true);
    upgrade_type_ = upgrade_type;
    package_url_ = package_url;
    ota_version_ = ota_version;
    OTA_LOG_INFO("接收升级任务: type={}, url={}, version={}", toString(upgrade_type),
                 package_url.empty() ? "(本地)" : package_url, ota_version.empty() ? "(未指定)" : ota_version);
    idle_task_waiter.notify();                               // 唤醒状态机线程
    return true;
  }

  void resetIsReceiveTask() {
    std::lock_guard<std::mutex> lock(mutex_receive_task_);
    is_receive_task_.store(false);
  }

  UpgradeType getUpgradeType() {
    std::lock_guard<std::mutex> lock(mutex_receive_task_);
    return upgrade_type_;
  }
  std::string getPackageUrl() {
    std::lock_guard<std::mutex> lock(mutex_receive_task_);
    return package_url_;
  }
  std::string getOtaVersion() {
    std::lock_guard<std::mutex> lock(mutex_receive_task_);
    return ota_version_;
  }
  void setUpgradeType(UpgradeType t) {
    std::lock_guard<std::mutex> lock(mutex_receive_task_);
    upgrade_type_ = t;
  }
  void setPackageUrl(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_receive_task_);
    package_url_ = url;
  }
  void setOtaVersion(const std::string& v) {
    std::lock_guard<std::mutex> lock(mutex_receive_task_);
    ota_version_ = v;
  }

  // ---- 失败复位（对应 /ota_reset 服务）----
  bool getIsFailedReset() { return is_failed_reset_.load(); }

  bool setIsFailedReset() {
    std::lock_guard<std::mutex> lock(mutex_failed_reset_);
    if (state_machine_type.load() != StateMachineType::FAILED) {
      OTA_LOG_WARN("拒绝复位：当前状态不是 FAILED");
      return false;
    }
    is_failed_reset_.store(true);
    OTA_LOG_INFO("收到失败复位请求");
    failed_task_waiter.notify();
    return true;
  }

  bool resetFailed() {
    std::lock_guard<std::mutex> lock(mutex_failed_reset_);
    if (!is_failed_reset_.load()) {
      return false;
    }
    is_failed_reset_.store(false);
    return true;
  }

  // ---- 任务列表 / 进度 ----
  std::vector<TaskType> task_type_list;
  std::atomic<int> current_task_index{0};

  // ---- 幂等标志（防止重复执行昂贵的操作）----
  std::atomic_bool is_had_send_rk_img{false};
  std::atomic_bool is_had_stop_controller{false};
  std::atomic_bool is_had_reboot{false};

  // ---- 线程池 ----
  std::shared_ptr<ThreadPool> thread_pool;

  // ---- 等待器 ----
  Waiter idle_task_waiter;
  Waiter failed_task_waiter;

  // ---- 路径工具（都基于 configures.work_dir）----
  Paths paths;

 private:
  std::mutex mutex_receive_task_;
  std::atomic_bool is_receive_task_{false};
  UpgradeType upgrade_type_ = UpgradeType::UNKNOWN;
  std::string package_url_;
  std::string ota_version_;

  std::mutex mutex_failed_reset_;
  std::atomic_bool is_failed_reset_{false};
};

// ===========================================================================
// 7) 状态转移表（对应 boost::sml 的 make_transition_table）
//    非法转移 = 直接 std::terminate()，与真实工程行为一致
// ===========================================================================
bool isTransitionAllowed(StateMachineType from, StateMachineType to);

// 生成状态机图（对应真实工程的 getStateMachinePlantUml）
std::string getStateMachinePlantUml();

// ===========================================================================
// 8) 状态基类
//    子类只写 run()；"推进状态 + 落盘 + 发 topic" 全在基类析构里统一做
// ===========================================================================
class StateMachineFunction {
 public:
  explicit StateMachineFunction(StateMachineType state_machine_type);
  virtual ~StateMachineFunction();

  virtual void run() = 0;

  static StateMachineData& data();

  StateMachineType stateType() const { return current_state_machine_type_; }

 protected:
  void setNextStateMachineType(StateMachineType type, int8_t ota_state, const std::string& msg = {});

  // 持久化辅助（对应基类里那一堆 write/get 函数）
  void writeCurrentStateMachineType();
  StateMachineType readStateMachineTypeFromFile() const;
  void writeUpgradeType(UpgradeType type);
  UpgradeType readUpgradeTypeFromFile() const;
  void writeTaskTypeList(const std::vector<TaskType>& list) const;
  std::vector<TaskType> readTaskTypeListFromFile() const;
  void writeTaskIndex(int index) const;
  int readTaskIndexFromFile() const;

  // 语音播报（对应 utils::ros2::callTTS）
  void say(const std::string& text);

  void publishStatus();

 private:
  StateMachineType current_state_machine_type_;
  StateMachineType next_state_machine_type_ = StateMachineType::INIT;
  int8_t next_ota_state_ = ota_state::IDLE;
};

}  // namespace ota_mini::state_machine
