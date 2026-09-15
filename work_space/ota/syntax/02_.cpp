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
// 枚举
#ifdef X
    #undef X
#endif
#define X(name,desc,desc_cn,value) name = value,
enum StateMachineType{
    OTA_STATE_MACHINE_LIST
};
#undef X

// 英文map
#define X(name,desc,desc_cn,value) {name,desc},
static std::unordered_map<StateMachineType,std::string> kStateToDesc = {
    OTA_STATE_MACHINE_LIST
};
#undef X

// 展开成中文map
#define X(name,desc,desc_cn,value) {name,desc_cn},
static std::unordered_map<StateMachineType,std::string> kStateToDescCn = {
    OTA_STATE_MACHINE_LIST
};
#undef X

#define X(name,desc,desc_cn,value) {desc,name},
static std::unordered_map<std::string,StateMachineType> kDescToState = {
    OTA_STATE_MACHINE_LIST
};
#undef X
#define X(name, desc, desc_cn, value) static_cast<StateMachineType>(value),

static std::unordered_set<StateMachineType> kDefinedStates = {
    OTA_STATE_MACHINE_LIST};

#undef X

const std::string & toString(StateMachineType code,bool is_cn = false) {
    const auto &table = is_cn ? kStateToDescCn : kStateToDesc;
    const auto it = table.find(code);
    if(it == table.end()) {
        throw std::invalid_argument("Invalid state machine :" + std::to_string(static_cast<int>(code)));
    }
    return it->second;
}

StateMachineType stringTo(const std::string& type) {
    std::string lower = type;
    std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if(kDescToState.find(lower) == kDescToState.end()){
        throw std::invalid_argument("Invalid state machine string:" + type);
    }
    return kDescToState[lower];
}

bool isDefined(StateMachineType code) {
    return kDefinedStates.find(code) != kDefinedStates.end();
}
}


namespace ota::task {
#ifdef X
    #undef X
#endif

#define X(name,desc,desc_cn,value) name = value,
enum TaskType {
    OTA_TASK_TYPE_LIST
};
#undef X

#define X(name,desc,desc_cn,value) {name,desc_cn},
static std::unordered_map<TaskType,std::string>
kTaskTypeToDescCn = {
    OTA_TASK_TYPE_LIST
};
#undef X


std::vector<TaskType> upgradeSequence() {
  // 对应 yaml 里的 upgrade_sequence: [nx_serial, rk_ethercat, rk_self, nx_self]
  return {TaskType::NX_SERIAL, TaskType::RK_ETHERCAT, TaskType::RK_SELF, TaskType::NX_SELF};
}

std::string toString(TaskType t) {
  const auto it = kTaskTypeToDescCn.find(t);
  return it == kTaskTypeToDescCn.end() ? "UNKNOWN" : it->second;
}
}


int main() {
    using namespace ota;
    printTitle("02 X-Macro");
    printSourceHint("src/ota/state_machine/state_machine.hpp|.cpp, src/ota/task/task.hpp|.cpp");
    MINI_INFO("{} -> {} / {}", static_cast<int>(state_machine::StateMachineType::CHECK_PREREQUISITES),
    state_machine::toString(state_machine::StateMachineType::CHECK_PREREQUISITES),
    state_machine::toString(state_machine::StateMachineType::CHECK_PREREQUISITES, true));
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
}

