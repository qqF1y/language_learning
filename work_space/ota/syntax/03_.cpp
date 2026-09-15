#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "mini_log.hpp"

namespace ota {
enum StateMachineType {
    INIT = -1,
    IDLE = 0,
    DOWNLOAD = 2,
    PARSING_IMG = 3,
    SUCCESS = 6,
    FAILED = 7,
};
enum class UpgradeType : std::int8_t {
    UNKNOWN = -1,
    LOCAL_NX_SERIAL = 0,
    LOCAL_RK_SERIAL = 3,
    LOCAL_ALL = 6,
    CLOUD = 7,
};

const char *toString(UpgradeType t) {
    switch(t) {
        case UpgradeType::UNKNOWN: return "unknown";
        case UpgradeType::LOCAL_NX_SERIAL: return "local_nx_serial";
        case UpgradeType::LOCAL_RK_SERIAL: return "local_rk_serial";
        case UpgradeType::LOCAL_ALL: return "local_all";
        case UpgradeType::CLOUD: return "cloud";
        default: return "unknown";
    }
    return "invalid";
}

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

class StateMachineData {
    public:
    std::atomic<StateMachineType> state_machine_type{INIT};
    std::atomic<UpgradeType> upgrade_type{UpgradeType::UNKNOWN};
    std::atomic<int> current_task_index{0};

    bool tryAdvanceIsReceiveTask() {
        bool expected = false;
        return is_receive_task.compare_exchange_weak(expected,true);
    }
    std::atomic<bool> is_receive_task{false};
};

}


int main() {
    using namespace ota;
    printTitle("03 enum /enum class/ std::atomic<enum>");
    printSourceHint("src/ota/syntax/03_.cpp");

    printStep("1)")
}