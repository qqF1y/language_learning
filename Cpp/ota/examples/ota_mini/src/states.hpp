/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/states.hpp
 * @Brief: 全部具体状态类 —— 对应真实工程 src/ota/state_machine 目录下的各状态文件
 *
 * 每个状态一个类，继承 StateMachineFunction，只实现 run()：
 *   - run() 里干活（下载/解析/并行升级/执行脚本……）
 *   - 结束时调 setNextStateMachineType(...) 声明下一个状态
 *   - 真正的推进/落盘/发 topic 由基类析构统一完成
 ***************************************************************/
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "state_machine.hpp"

namespace ota_mini::state_machine {

// 全局唯一的"状态工厂"：StateMachineType -> 具体状态对象
// 对应真实工程 ota.cpp 里的 state_machine_list
std::unique_ptr<StateMachineFunction> createState(StateMachineType type);

// ---------------------------------------------------------------------------
// 各状态声明。全部 final：状态类不应该再被继承。
// ---------------------------------------------------------------------------
class Init final : public StateMachineFunction {
 public:
  Init() : StateMachineFunction(StateMachineType::INIT) {}
  void run() override;
};

class Idle final : public StateMachineFunction {
 public:
  Idle() : StateMachineFunction(StateMachineType::IDLE) {}
  void run() override;
};

class CheckPrerequisites final : public StateMachineFunction {
 public:
  CheckPrerequisites() : StateMachineFunction(StateMachineType::CHECK_PREREQUISITES) {}
  void run() override;
};

class Download final : public StateMachineFunction {
 public:
  Download() : StateMachineFunction(StateMachineType::DOWNLOAD) {}
  void run() override;

 private:
  // 模拟 libcurl 下载：真正下载会走 utils::curl
  bool simulateDownload(const std::string& url, const std::string& save_path);
};

class ParsingImg final : public StateMachineFunction {
 public:
  ParsingImg() : StateMachineFunction(StateMachineType::PARSING_IMG) {}
  void run() override;

 private:
  std::vector<TaskType> makeTaskList();
  bool hasArtifacts(TaskType type) const;
  std::string artifactDir(TaskType type) const;
  std::string artifactExtension(TaskType type) const;
};

class CheckOtaTask final : public StateMachineFunction {
 public:
  CheckOtaTask() : StateMachineFunction(StateMachineType::CHECK_OTA_TASK) {}
  void run() override;

 private:
  bool sendRkImageToRk3588();
};

class SendImgData final : public StateMachineFunction {
 public:
  SendImgData() : StateMachineFunction(StateMachineType::SEND_IMG_DATA) {}
  void run() override;
};

class NxSelf final : public StateMachineFunction {
 public:
  NxSelf() : StateMachineFunction(StateMachineType::NX_SELF) {}
  void run() override;
};

class RkSendOtaCmd final : public StateMachineFunction {
 public:
  RkSendOtaCmd() : StateMachineFunction(StateMachineType::RK_SEND_OTA_CMD) {}
  void run() override;

 private:
  bool upgradeEthercatMotors();
};

class Success final : public StateMachineFunction {
 public:
  Success() : StateMachineFunction(StateMachineType::SUCCESS) {}
  void run() override;
};

class Failed final : public StateMachineFunction {
 public:
  Failed() : StateMachineFunction(StateMachineType::FAILED) {}
  void run() override;
};

class Reboot final : public StateMachineFunction {
 public:
  Reboot() : StateMachineFunction(StateMachineType::REBOOT) {}
  void run() override;

 private:
  void writeVersionJson();
};

class PubHardReboot final : public StateMachineFunction {
 public:
  PubHardReboot() : StateMachineFunction(StateMachineType::PUB_HARD_REBOOT) {}
  void run() override;
};

}  // namespace ota_mini::state_machine
