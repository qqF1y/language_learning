/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/main.cpp
 * @Brief: 入口 + 交互式命令行（模拟 ros2 service call）
 *
 * 真实工程用法：
 *   ros2 service call /ota_local common_msgs/srv/BaseString "{data: \"ALL\"}"
 *   ros2 service call /ota_eame  common_msgs/srv/OtaCmd "{version: \"v1.0\", url: \"https://...\"}"
 *   ros2 service call /ota_reset common_msgs/srv/SetInt8 "{data: 0}"
 *   ros2 topic echo /ota_status
 *
 * 本例程把这些命令搬到一个终端里，方便直接观察状态机流转。
 ***************************************************************/

#include <chrono>
#include <cstring>
#include <functional>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "log.hpp"
#include "ota_node.hpp"
#include "states.hpp"

#ifndef OTA_MINI_DEFAULT_CONFIG
  #define OTA_MINI_DEFAULT_CONFIG "configures/ota_mini.yaml"
#endif

namespace {

using ota_mini::OtaNode;
using ota_mini::state_machine::StateMachineType;

constexpr const char* kBanner = R"(
=========================================================================
 ota_mini —— 人形机器人 OTA 状态机（综合例程）
 对应真实工程: /home/qiurangfei/PRJ/ota/humanoid_ota
=========================================================================
 可用命令（等价于 ros2 service call / topic echo）:

   local <ALL|RK_MOTOR|RK_MCU|RK_SELF|NX_SELF>   触发本地升级 (/ota_local)
   cloud <version> <url>                        触发云端升级 (/ota_eame)
   reset                                        复位失败状态 (/ota_reset)
   status                                       查看 /ota_status

   battery <0-100>                              模拟 /battery_state 消息
   hang <0|1>                                   模拟 /manager/robot_hanged 消息

   help / quit
=========================================================================
)";

const char* stateName(StateMachineType t) { return ota_mini::state_machine::toString(t).c_str(); }

// 阻塞等待条件成立，超时返回 false
bool waitUntil(const std::function<bool()>& condition, int timeout_ms, int poll_ms = 50) {
  const int loops = timeout_ms / poll_ms;
  for (int i = 0; i < loops; ++i) {
    if (condition()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
  }
  return condition();
}

// 只在打印时取一次
void printStatus(const OtaNode& node) {
  const auto status = node.lastOtaStatus();
  std::cout << "  state_machine = " << stateName(node.currentState()) << "\n"
            << "  ota_state     = " << static_cast<int>(status.state) << "\n"
            << "  details       = " << (status.state_details.empty() ? "-" : status.state_details) << "\n"
            << "  progress      = " << status.pkg_current << "/" << status.pkg_total << "\n"
            << "  download      = " << status.download_progress << "%\n";
}

// 等回到 IDLE；如果中途进入 FAILED，就自动发一次 /ota_reset
void waitForIdleWithAutoReset(OtaNode& node, int timeout_ms) {
  const bool ok = waitUntil(
      [&node] {
        const auto state = node.currentState();
        if (state == StateMachineType::FAILED) {
          std::cout << "  [demo] 检测到 FAILED，自动发送 /ota_reset ……\n";
          std::string err;
          node.srvOtaReset(&err);
        }
        return state == StateMachineType::IDLE || !ota_mini::state_machine::StateMachineFunction::data().running.load();
      },
      timeout_ms, 50);

  if (!ok) {
    std::cout << "  [demo] 等待回到 IDLE 超时\n";
  }
}

// 发起一次升级并等它跑完（包含：离开 IDLE -> 回到 IDLE）
void runUpgradeCycle(OtaNode& node, const std::string& label, bool submitted) {
  std::cout << "\n---------- " << label << " ----------\n";
  if (!submitted) {
    std::cout << "  任务未被接受（见上面日志），跳过等待\n";
    return;
  }
  // 先等状态机离开 IDLE（说明任务真的开始跑了）
  waitUntil([&node] { return node.currentState() != StateMachineType::IDLE; }, 3000);
  waitForIdleWithAutoReset(node, 120000);
  std::cout << "  结束后状态 = " << stateName(node.currentState()) << "\n";
}

// ---------------------------------------------------------------------------
// demo 模式：无人值守跑完一串升级，方便一次性看完整流程
// ---------------------------------------------------------------------------
void runDemo(OtaNode& node) {
  std::cout << "\n########## DEMO 模式：自动执行一串升级 ##########\n";

  std::cout << "\n[demo] 等待状态机进入 IDLE ……\n";
  waitUntil([&node] { return node.currentState() == StateMachineType::IDLE; }, 5000);

  // ---- 1) 本地整包升级（local ALL）----
  {
    std::string err;
    const bool ok = node.srvOtaLocalCmd("ALL", &err);
    runUpgradeCycle(node, "1) 本地整包升级  local ALL", ok);
  }

  // ---- 2) 云端升级（cloud v9.9.9 ...）----
  {
    std::string err;
    const bool ok = node.srvOtaCloudCmd("v9.9.9", "https://ota.example.com/media/img.zip", &err);
    runUpgradeCycle(node, "2) 云端升级  cloud v9.9.9 <url>", ok);
  }

  // ---- 3) 电量不足，服务应拒绝 ----
  {
    std::cout << "\n---------- 3) 低电量保护 ----------\n";
    node.subBatteryState(10);
    std::string err;
    const bool ok = node.srvOtaLocalCmd("ALL", &err);
    std::cout << "  /ota_local 返回 " << (ok ? "成功" : "失败: " + err) << "\n";
    node.subBatteryState(90);
  }

  // ---- 4) 仅升级 RK3588 自身 ----
  {
    std::string err;
    const bool ok = node.srvOtaLocalCmd("RK_SELF", &err);
    runUpgradeCycle(node, "4) RK 自升级  local RK_SELF", ok);
  }

  // ---- 5) 单文件升级（.bin）----
  {
    std::string err;
    const bool ok = node.srvOtaLocalCmd("mcu_v1.0.4.bin", &err);
    runUpgradeCycle(node, "5) 单文件升级  mcu_v1.0.4.bin", ok);
  }

  std::cout << "\n########## DEMO 结束 ##########\n";
  std::cout << "最终 /ota_status:\n";
  printStatus(node);

  // 打印落盘的版本号文件，验证 Reboot 状态的收尾逻辑
  const auto version_file = ota_mini::state_machine::StateMachineFunction::data().paths.versionJsonFile();
  std::ifstream ifs(version_file);
  if (ifs.is_open()) {
    std::stringstream ss;
    ss << ifs.rdbuf();
    std::cout << "版本号文件 " << version_file.string() << " 内容: " << ss.str() << "\n";
  }
}

// ---------------------------------------------------------------------------
// 交互模式
// ---------------------------------------------------------------------------
void runInteractive(OtaNode& node) {
  std::cout << kBanner << "\n";

  std::string line;
  while (true) {
    std::cout << "\nota_mini> " << std::flush;
    if (!std::getline(std::cin, line)) {
      break;   // EOF（比如 Ctrl-D）
    }
    if (line.empty()) {
      continue;
    }

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "quit" || cmd == "exit" || cmd == "q") {
      break;
    }

    if (cmd == "help" || cmd == "h") {
      std::cout << kBanner;
      continue;
    }

    if (cmd == "status" || cmd == "s") {
      std::cout << "当前状态: " << stateName(node.currentState()) << "\n";
      printStatus(node);
      continue;
    }

    if (cmd == "battery") {
      int value = 0;
      iss >> value;
      node.subBatteryState(value);
      continue;
    }

    if (cmd == "hang") {
      int value = 0;
      iss >> value;
      node.subIsHangUp(value != 0);
      continue;
    }

    if (cmd == "local") {
      std::string arg;
      iss >> arg;
      std::string err;
      const bool ok = node.srvOtaLocalCmd(arg, &err);
      std::cout << "  -> " << (ok ? "接收任务成功" : ("接收任务失败: " + err)) << "\n";
      if (ok) {
        // 交互模式下不阻塞，让用户手动敲 status 观察
        std::cout << "  （任务已下发，输入 status 查看进度）\n";
      }
      continue;
    }

    if (cmd == "cloud") {
      std::string version;
      std::string url;
      iss >> version >> url;
      std::string err;
      const bool ok = node.srvOtaCloudCmd(version, url, &err);
      std::cout << "  -> " << (ok ? "接收任务成功" : ("接收任务失败: " + err)) << "\n";
      continue;
    }

    if (cmd == "reset") {
      std::string err;
      const bool ok = node.srvOtaReset(&err);
      std::cout << "  -> " << (ok ? "复位成功" : ("复位失败: " + err)) << "\n";
      continue;
    }

    std::cout << "未知命令: " << cmd << "（输入 help 查看帮助）\n";
  }

  std::cout << "\n收到退出指令，正在停止 OtaNode ……\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string config_path = OTA_MINI_DEFAULT_CONFIG;
  bool demo = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--demo") {
      demo = true;
    } else if (arg == "--config" && i + 1 < argc) {
      config_path = argv[++i];
    } else if (arg == "--quiet") {
      ota_mini::globalLogLevel() = ota_mini::LogLevel::Warn;
    } else if (arg == "-h" || arg == "--help") {
      std::cout << "用法: ota_mini [--demo] [--config <yaml>] [--quiet]\n";
      return 0;
    } else {
      std::cerr << "未知参数: " << arg << "\n";
      return 1;
    }
  }

  std::cout << "使用配置: " << config_path << "\n";

  OtaNode node(config_path);
  if (!node.start()) {
    std::cerr << "启动失败，请检查配置文件路径\n";
    return 1;
  }

  if (demo) {
    runDemo(node);
  } else {
    runInteractive(node);
  }

  node.stop();   // 析构函数也会调，但显式调一次更清楚
  std::cout << "进程退出。\n";
  return 0;
}
