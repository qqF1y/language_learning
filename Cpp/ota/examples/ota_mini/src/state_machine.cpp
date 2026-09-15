/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/state_machine.cpp
 * @Brief: 状态机框架实现：
 *          X-Macro 展开成映射表 -> 转移表校验 -> 基类析构推进状态
 ***************************************************************/

#include "state_machine.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "fs_utils.hpp"
#include "log.hpp"

namespace ota_mini::state_machine {
namespace {

// ---------------------------------------------------------------------------
// 用 X-Macro 从同一份列表生成 4 张表（详见 syntax/02_xmacro_enum.cpp）
// ---------------------------------------------------------------------------
#ifdef X
  #undef X
#endif

#define X(name, desc, desc_cn, value) {StateMachineType::name, desc},
const std::unordered_map<StateMachineType, std::string> kStateToDesc = {OTA_STATE_LIST};
#undef X

#define X(name, desc, desc_cn, value) {StateMachineType::name, desc_cn},
const std::unordered_map<StateMachineType, std::string> kStateToDescCn = {OTA_STATE_LIST};
#undef X

#define X(name, desc, desc_cn, value) {desc, StateMachineType::name},
const std::unordered_map<std::string, StateMachineType> kDescToState = {OTA_STATE_LIST};
#undef X

#define X(name, desc, desc_cn, value) static_cast<StateMachineType>(value),
const std::unordered_set<StateMachineType> kDefinedStates = {OTA_STATE_LIST};
#undef X

// TaskType 同理
#define X(name, desc, desc_cn, value) {TaskType::name, desc},
const std::unordered_map<TaskType, std::string> kTaskToDesc = {OTA_TASK_TYPE_LIST};
#undef X

#define X(name, desc, desc_cn, value) {TaskType::name, desc_cn},
const std::unordered_map<TaskType, std::string> kTaskToDescCn = {OTA_TASK_TYPE_LIST};
#undef X

#define X(name, desc, desc_cn, value) {desc, TaskType::name},
const std::unordered_map<std::string, TaskType> kDescToTask = {OTA_TASK_TYPE_LIST};
#undef X

#define X(name, desc, desc_cn, value) static_cast<TaskType>(value),
const std::unordered_set<TaskType> kDefinedTasks = {OTA_TASK_TYPE_LIST};
#undef X

// ---------------------------------------------------------------------------
// 状态转移表：等价于 boost::sml 的
//   *"init"_s + event<EventIdle> = "idle"_s
//     , "idle"_s + event<EventCheckPrerequisites> = "check_prerequisites"_s
//     ...
// 用一张 (from, to) 的集合表达，效果一样：不在表里的转移就是非法转移。
// ---------------------------------------------------------------------------
struct Transition {
  StateMachineType from;
  StateMachineType to;
};

constexpr const char* kStateNameShort(StateMachineType t) {
  switch (t) {
    case StateMachineType::INIT: return "init";
    case StateMachineType::IDLE: return "idle";
    case StateMachineType::CHECK_PREREQUISITES: return "check_prerequisites";
    case StateMachineType::DOWNLOAD: return "download";
    case StateMachineType::PARSING_IMG: return "parsing_img";
    case StateMachineType::CHECK_OTA_TASK: return "check_ota_task";
    case StateMachineType::SEND_IMG_DATA: return "send_img_data";
    case StateMachineType::NX_SELF: return "nx_self";
    case StateMachineType::RK_SEND_OTA_CMD: return "rk_send_ota_cmd";
    case StateMachineType::SUCCESS: return "success";
    case StateMachineType::FAILED: return "failed";
    case StateMachineType::REBOOT: return "reboot";
    case StateMachineType::PUB_HARD_REBOOT: return "pub_hard_reboot";
  }
  return "?";
}

// 用 X-Macro 定义转移表：T(from, to)
#define OTA_TRANSITION_LIST                     \
  T(INIT, IDLE)                                 \
  T(INIT, REBOOT)                               \
  T(IDLE, CHECK_PREREQUISITES)                  \
  T(CHECK_PREREQUISITES, CHECK_PREREQUISITES)   \
  T(CHECK_PREREQUISITES, DOWNLOAD)              \
  T(CHECK_PREREQUISITES, PARSING_IMG)           \
  T(CHECK_PREREQUISITES, FAILED)                \
  T(DOWNLOAD, PARSING_IMG)                      \
  T(DOWNLOAD, FAILED)                           \
  T(PARSING_IMG, CHECK_OTA_TASK)                \
  T(PARSING_IMG, FAILED)                        \
  T(CHECK_OTA_TASK, IDLE)                       \
  T(CHECK_OTA_TASK, SEND_IMG_DATA)              \
  T(CHECK_OTA_TASK, NX_SELF)                    \
  T(CHECK_OTA_TASK, RK_SEND_OTA_CMD)            \
  T(CHECK_OTA_TASK, FAILED)                     \
  T(SEND_IMG_DATA, SUCCESS)                     \
  T(SEND_IMG_DATA, FAILED)                      \
  T(NX_SELF, SUCCESS)                           \
  T(NX_SELF, FAILED)                            \
  T(RK_SEND_OTA_CMD, SUCCESS)                   \
  T(RK_SEND_OTA_CMD, FAILED)                    \
  T(SUCCESS, CHECK_OTA_TASK)                    \
  T(SUCCESS, REBOOT)                            \
  T(REBOOT, IDLE)                               \
  T(REBOOT, PUB_HARD_REBOOT)                    \
  T(PUB_HARD_REBOOT, IDLE)                      \
  T(FAILED, IDLE)

// 注意：
//   1. CHECK_PREREQUISITES -> CHECK_PREREQUISITES 这条自环，是为了支持
//      "挂起状态没满足就 1 秒后重试"。真实工程的 sml 转移表里漏了这条边，
//      所以它的重试路径其实是会 terminate 的（见 01_工程总结.md 第 11 节）。
//   2. 这里没有 SUCCESS -> FAILED、REBOOT -> FAILED 等边。
//      真实工程就因为没给 nx_self / stop_related_node / open_tty 写 FAILED 边，
//      一旦这些状态失败，process_event 返回 false，直接 std::terminate() 崩进程。
//      本例程保留同样"严格"的语义，并给出清晰报错。

#define T(from, to) {StateMachineType::from, StateMachineType::to},
const std::vector<Transition> kTransitions = {OTA_TRANSITION_LIST};
#undef T

}  // namespace

// ===========================================================================
// toString / stringTo / isDefined
// ===========================================================================
const std::string& toString(StateMachineType type, bool is_cn) {
  const auto& table = is_cn ? kStateToDescCn : kStateToDesc;
  const auto it = table.find(type);
  if (it == table.end()) {
    throw std::invalid_argument("invalid StateMachineType: " + std::to_string(static_cast<int>(type)));
  }
  return it->second;
}

StateMachineType stringTo(const std::string& type) {
  std::string lower = type;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  const auto it = kDescToState.find(lower);
  if (it == kDescToState.end()) {
    throw std::invalid_argument("invalid state name: " + type);
  }
  return it->second;
}

bool isDefined(StateMachineType type) { return kDefinedStates.count(type) != 0; }

const std::string& toString(TaskType type, bool is_cn) {
  const auto& table = is_cn ? kTaskToDescCn : kTaskToDesc;
  const auto it = table.find(type);
  if (it == table.end()) {
    throw std::invalid_argument("invalid TaskType: " + std::to_string(static_cast<int>(type)));
  }
  return it->second;
}

TaskType stringToTaskType(const std::string& type) {
  std::string lower = type;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  const auto it = kDescToTask.find(lower);
  return it == kDescToTask.end() ? TaskType::UNKNOWN : it->second;
}

bool isDefined(TaskType type) { return kDefinedTasks.count(type) != 0; }

const std::string& toString(UpgradeType type) {
  static const std::unordered_map<int, std::string> table = {
      {static_cast<int>(UpgradeType::UNKNOWN), "unknown"},
      {static_cast<int>(UpgradeType::LOCAL_ALL), "local_all"},
      {static_cast<int>(UpgradeType::LOCAL_RK_MOTOR), "local_rk_motor"},
      {static_cast<int>(UpgradeType::LOCAL_RK_MCU), "local_rk_mcu"},
      {static_cast<int>(UpgradeType::LOCAL_RK_SELF), "local_rk_self"},
      {static_cast<int>(UpgradeType::LOCAL_NX_SELF), "local_nx_self"},
      {static_cast<int>(UpgradeType::CLOUD), "cloud"},
  };
  static const std::string unknown = "invalid";
  const auto it = table.find(static_cast<int>(type));
  return it == table.end() ? unknown : it->second;
}

// ===========================================================================
// 转移表
// ===========================================================================
bool isTransitionAllowed(StateMachineType from, StateMachineType to) {
  return std::any_of(kTransitions.begin(), kTransitions.end(),
                     [from, to](const Transition& t) { return t.from == from && t.to == to; });
}

std::string getStateMachinePlantUml() {
  std::ostringstream oss;
  oss << "@startuml\n";
  oss << "[*] --> init\n";
  for (const auto& t : kTransitions) {
    oss << kStateNameShort(t.from) << " --> " << kStateNameShort(t.to) << "\n";
  }
  oss << "@enduml";
  return oss.str();
}

// ===========================================================================
// StateMachineFunction
// ===========================================================================
StateMachineData& StateMachineFunction::data() {
  static StateMachineData instance;   // magic static：线程安全、只构造一次
  return instance;
}

StateMachineFunction::StateMachineFunction(StateMachineType state_machine_type)
    : current_state_machine_type_(state_machine_type) {
  // REBOOT 会重复进入，用 DEBUG 级别避免刷屏（对应真实工程的 MJR_INFO_ONCE）
  OTA_LOG_INFO(">>> 进入状态: {}", toString(state_machine_type));
}

void StateMachineFunction::setNextStateMachineType(StateMachineType type, int8_t ota_state,
                                                    const std::string& msg) {
  next_state_machine_type_ = type;
  next_ota_state_ = ota_state;
  data().ota_state_msg.state = ota_state;
  data().ota_state_msg.state_details = msg;
}

StateMachineFunction::~StateMachineFunction() {
  auto& d = data();

  // 进程要退出了，不再推进状态（对应真实工程里 if (!rclcpp::ok()) return;）
  if (!d.running.load()) {
    OTA_LOG_INFO("进程终止，跳过状态推进");
    return;
  }

  OTA_LOG_INFO("<<< 结束状态: {} -> {}", toString(current_state_machine_type_),
               toString(next_state_machine_type_));

  // ---- 关键：转移合法性校验（boost::sml 的行为）----
  if (!isTransitionAllowed(current_state_machine_type_, next_state_machine_type_)) {
    OTA_LOG_ERROR("非法状态转移: {} -> {}，终止进程（转移表里缺少这条边）",
                  toString(current_state_machine_type_), toString(next_state_machine_type_));
    std::terminate();
  }

  d.last_state_machine_type = d.state_machine_type.load();
  d.state_machine_type.store(next_state_machine_type_);

  writeCurrentStateMachineType();
  publishStatus();
}

void StateMachineFunction::writeCurrentStateMachineType() {
  const auto path = data().paths.stateFile();
  const auto text = std::to_string(static_cast<int>(data().state_machine_type.load()));
  if (auto ret = fs::atomicWriteFile(path, text); !ret.has_value()) {
    OTA_LOG_ERROR("写状态文件失败: {}", path.string());
    std::terminate();
  }
  OTA_LOG_DEBUG("状态已落盘: {} = {}", path.string(), text);
}

StateMachineType StateMachineFunction::readStateMachineTypeFromFile() const {
  const auto path = data().paths.stateFile();
  auto content = fs::readFile(path);
  if (!content.has_value()) {
    return StateMachineType::INIT;
  }
  try {
    const auto value = static_cast<StateMachineType>(std::stoi(content.value()));
    return isDefined(value) ? value : StateMachineType::INIT;
  } catch (const std::exception& e) {
    OTA_LOG_ERROR("状态文件内容非法: {} ({})", content.value(), e.what());
    return StateMachineType::INIT;
  }
}

void StateMachineFunction::writeUpgradeType(UpgradeType type) {
  const auto path = data().paths.upgradeTypeFile();
  if (auto ret = fs::atomicWriteFile(path, std::to_string(static_cast<int>(type))); !ret.has_value()) {
    OTA_LOG_ERROR("写升级类型文件失败: {}", path.string());
    std::terminate();
  }
}

UpgradeType StateMachineFunction::readUpgradeTypeFromFile() const {
  auto content = fs::readFile(data().paths.upgradeTypeFile());
  if (!content.has_value()) {
    return UpgradeType::UNKNOWN;
  }
  try {
    return static_cast<UpgradeType>(std::stoi(content.value()));
  } catch (const std::exception&) {
    return UpgradeType::UNKNOWN;
  }
}

void StateMachineFunction::writeTaskTypeList(const std::vector<TaskType>& list) const {
  std::string text;
  for (const auto& t : list) {
    text += toString(t);
    text += " ";
  }
  const auto path = data().paths.taskListFile();
  if (auto ret = fs::atomicWriteFile(path, text); !ret.has_value()) {
    OTA_LOG_ERROR("写任务列表文件失败: {}", path.string());
    std::terminate();
  }
}

std::vector<TaskType> StateMachineFunction::readTaskTypeListFromFile() const {
  std::vector<TaskType> list;
  auto content = fs::readFile(data().paths.taskListFile());
  if (!content.has_value()) {
    return list;
  }
  std::istringstream iss(content.value());
  std::string token;
  while (iss >> token) {                        // 对应 task.cpp 的 while (ss >> task_type_str)
    const auto t = stringToTaskType(token);
    if (isDefined(t)) {
      list.push_back(t);
    } else {
      OTA_LOG_WARN("任务列表里有无法识别的类型: {}", token);
    }
  }
  return list;
}

void StateMachineFunction::writeTaskIndex(int index) const {
  const auto path = data().paths.taskIndexFile();
  if (auto ret = fs::atomicWriteFile(path, std::to_string(index)); !ret.has_value()) {
    OTA_LOG_ERROR("写任务索引文件失败: {}", path.string());
    std::terminate();
  }
}

int StateMachineFunction::readTaskIndexFromFile() const {
  auto content = fs::readFile(data().paths.taskIndexFile());
  if (!content.has_value()) {
    return 0;
  }
  try {
    return std::stoi(content.value());
  } catch (const std::exception&) {
    return 0;
  }
}

void StateMachineFunction::say(const std::string& text) {
  if (data().speak) {
    data().speak(text);
  }
}

void StateMachineFunction::publishStatus() {
  auto& d = data();
  d.ota_state_msg.pkg_current = d.current_task_index.load();
  d.ota_state_msg.pkg_total = static_cast<int>(d.task_type_list.size());
  if (d.publish_status) {
    d.publish_status(d.ota_state_msg);
  }
}

}  // namespace ota_mini::state_machine
