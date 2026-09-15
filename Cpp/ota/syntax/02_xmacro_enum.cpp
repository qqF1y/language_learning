/*************************************************************
 * @FilePath: /Cpp/ota/syntax/02_xmacro_enum.cpp
 * @Brief: 语法点 02 —— X-Macro：一份数据源生成枚举/字符串/解析/校验
 *
 * 真实工程出处：
 *   - src/ota/state_machine/state_machine.hpp   OTA_STATE_MACHINE_LIST
 *   - src/ota/state_machine/state_machine.cpp   OTA_STATE_MACHINE_LIST 的 4 种展开
 *   - src/ota/task/task.hpp / task.cpp          OTA_TASK_TYPE_LIST
 *   - src/utils/[子模块]/error.hpp                OTA_ERROR_CODE_LIST
 *
 * 核心思想：
 *   枚举值、字符串描述、中文描述、数值、合法性校验，
 *   这些都来自**同一份列表**。手写 4 遍必然不一致，X-Macro 保证只有一处真相。
 *
 * 用法三步：
 *   1. #define X(...)  定义"怎么展开"
 *   2. 写 LIST 宏
 *   3. 用完立刻 #undef X，避免污染后面的宏展开
 ***************************************************************/

#include <algorithm>
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "mini_log.hpp"
#include "xmacro_list.hpp"

namespace ota::state_machine {

// ===========================================================================
// 展开 1：生成枚举本体
// ===========================================================================
#ifdef X
  #undef X
#endif

#define X(name, desc, desc_cn, value) name = value,

enum StateMachineType {
  OTA_STATE_MACHINE_LIST
};

#undef X  // ← 关键：用完立刻撤销，否则会污染下一个展开

// ===========================================================================
// 展开 2：生成 枚举 -> 英文描述 的 map
// ===========================================================================
#define X(name, desc, desc_cn, value) {name, desc},

static std::unordered_map<StateMachineType, std::string> kStateToDesc = {
    OTA_STATE_MACHINE_LIST};

#undef X

// ===========================================================================
// 展开 3：生成 枚举 -> 中文描述 的 map
//    注意：同一个 X 宏，这里只换了下映射内容，列表本身一个字都没改
// ===========================================================================
#define X(name, desc, desc_cn, value) {name, desc_cn},

static std::unordered_map<StateMachineType, std::string> kStateToDescCn = {
    OTA_STATE_MACHINE_LIST};

#undef X

// ===========================================================================
// 展开 4：生成 字符串 -> 枚举 的反查 map
// ===========================================================================
#define X(name, desc, desc_cn, value) {desc, name},

static std::unordered_map<std::string, StateMachineType> kDescToState = {
    OTA_STATE_MACHINE_LIST};

#undef X

// ===========================================================================
// 展开 5：生成"合法性校验"集合
// ===========================================================================
#define X(name, desc, desc_cn, value) static_cast<StateMachineType>(value),

static std::unordered_set<StateMachineType> kDefinedStates = {
    OTA_STATE_MACHINE_LIST};

#undef X

// ---------------------------------------------------------------------------
// 对外 API：和真实工程逐行对应的 toString / stringTo / isDefined
// ---------------------------------------------------------------------------
const std::string& toString(StateMachineType code, bool is_cn = false) {
  const auto& table = is_cn ? kStateToDescCn : kStateToDesc;
  const auto it = table.find(code);
  if (it == table.end()) {
    // 真实工程这里是 MJR_ERROR + throw std::invalid_argument
    throw std::invalid_argument("Invalid state machine: " + std::to_string(static_cast<int>(code)));
  }
  return it->second;  // 返回 map 内部元素的引用，合法且零拷贝
}

StateMachineType stringTo(const std::string& type) {
  // 真实工程 task.cpp 里会先转小写再查
  std::string lower = type;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (kDescToState.find(lower) == kDescToState.end()) {
    throw std::invalid_argument("Invalid state machine string: " + type);
  }
  return kDescToState[lower];
}

bool isDefined(StateMachineType code) {
  return kDefinedStates.find(code) != kDefinedStates.end();
}

}  // namespace ota::state_machine

namespace ota::task {

// 再演示一遍：任务类型也用同一套套路
#ifdef X
  #undef X
#endif

#define X(name, desc, desc_cn, value) name = value,
enum TaskType {
  OTA_TASK_TYPE_LIST
};
#undef X

#define X(name, desc, desc_cn, value) {name, desc_cn},
static std::unordered_map<TaskType, std::string> kTaskTypeToDescCn = {
    OTA_TASK_TYPE_LIST};
#undef X

std::vector<TaskType> upgradeSequence() {
  // 对应 yaml 里的 upgrade_sequence: [nx_serial, rk_ethercat, rk_self, nx_self]
  return {TaskType::NX_SERIAL, TaskType::RK_ETHERCAT, TaskType::RK_SELF, TaskType::NX_SELF};
}

std::string toString(TaskType t) {
  const auto it = kTaskTypeToDescCn.find(t);
  return it == kTaskTypeToDescCn.end() ? "UNKNOWN" : it->second;
}

}  // namespace ota::task

int main() {
  using namespace ota;
  printTitle("02 X-Macro 生成枚举/字符串表/校验");
  printSourceHint("src/ota/state_machine/state_machine.hpp|.cpp, src/ota/task/task.hpp|.cpp");

  printStep("1) 枚举 -> 英文/中文 描述");
  MINI_INFO("{} -> {} / {}", static_cast<int>(state_machine::StateMachineType::CHECK_PREREQUISITES),
            state_machine::toString(state_machine::StateMachineType::CHECK_PREREQUISITES),
            state_machine::toString(state_machine::StateMachineType::CHECK_PREREQUISITES, true));
  MINI_INFO("{} -> {}", static_cast<int>(state_machine::StateMachineType::SEND_IMG_DATA),
            state_machine::toString(state_machine::StateMachineType::SEND_IMG_DATA, true));

  printStep("2) 字符串 -> 枚举（反查，用于从磁盘状态文件恢复）");
  // 真实工程 INIT 状态就是从文件里读字符串/数字来恢复上一次中断的状态
  const auto restored = state_machine::stringTo("parsing_img");
  MINI_INFO("stringTo(\"parsing_img\") = {} ({})", static_cast<int>(restored),
            state_machine::toString(restored, true));

  printStep("3) 合法性校验（防止磁盘文件被改坏后越界）");
  MINI_INFO("isDefined(IDLE)      = {}", state_machine::isDefined(state_machine::StateMachineType::IDLE));
  MINI_INFO("isDefined((enum)999) = {}", state_machine::isDefined(static_cast<state_machine::StateMachineType>(999)));

  printStep("4) 非法输入的处理（异常路径）");
  try {
    (void)state_machine::stringTo("not_a_state");
  } catch (const std::invalid_argument& e) {
    MINI_ERROR("捕获异常: {}", e.what());
  }

  printStep("5) 任务类型 + 升级序列（对应 yaml 的 upgrade_sequence）");
  for (const auto& t : task::upgradeSequence()) {
    MINI_INFO("任务: {} -> {}", static_cast<int>(t), task::toString(t));
  }

  printStep("小结");
  std::cout << R"(
    - LIST 宏是唯一数据源；X 宏只决定"这一遍怎么展开"
    - 每次展开结束必须 #undef X（真实工程用 #ifdef X / #undef X 兜底）
    - 新增一个状态 = 列表里加一行，枚举/字符串/反查/校验四处自动同步
    - 反向查表用 description（英文），保证和枚举名解耦
)";
  return 0;
}
