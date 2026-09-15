/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/ota_node.hpp
 * @Brief: 节点装配层 —— 对应真实工程 src/ota/ota.hpp|.cpp 的 OtaNode
 *
 * 真实工程里 OtaNode 负责：
 *   - 创建 ROS2 订阅（/battery_state、/manager/robot_hanged）
 *   - 创建 ROS2 发布（/ota_status）和服务（/ota_local、/ota_eame、/ota_reset）
 *   - 创建 LCM 与 RK3588 通信
 *   - 起两个线程：状态机线程 + 100ms 周期任务线程
 *
 * 这里用"函数调用"模拟 ROS2 的 Service/Topic，但**线程模型完全一致**：
 *   回调线程只置标志 + notify，状态机线程自己醒来处理。
 ***************************************************************/
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include "config.hpp"
#include "state_machine.hpp"

namespace ota_mini {

class OtaNode {
 public:
  explicit OtaNode(std::string config_path);
  ~OtaNode();

  OtaNode(const OtaNode&) = delete;
  OtaNode& operator=(const OtaNode&) = delete;

  // 加载配置 + 起线程。失败返回 false。
  bool start();
  void stop();

  // ==========================================================================
  // 以下三个函数 = 真实工程的三个 ROS2 Service
  // ==========================================================================
  // ros2 service call /ota_local common_msgs/srv/BaseString "{data: "ALL"}"
  bool srvOtaLocalCmd(const std::string& data, std::string* err = nullptr);

  // ros2 service call /ota_eame common_msgs/srv/OtaCmd "{version: "v1", url: "https://..."}"
  bool srvOtaCloudCmd(const std::string& version, const std::string& url, std::string* err = nullptr);

  // ros2 service call /ota_reset common_msgs/srv/SetInt8 "{data: 0}"
  bool srvOtaReset(std::string* err = nullptr);

  // ==========================================================================
  // 以下两个函数 = 真实工程订阅的两个 Topic（模拟消息到达）
  // ==========================================================================
  void subBatteryState(int percentage);   // /battery_state
  void subIsHangUp(bool hang_up);         // /manager/robot_hanged

  // ==========================================================================
  // 查询（对应 ros2 topic echo /ota_status）
  // ==========================================================================
  state_machine::OtaStateMsg lastOtaStatus() const;
  state_machine::StateMachineType currentState() const;

  const config::Config& configure() const { return config_; }
  const std::string& configPath() const { return config_path_; }

 private:
  // 状态机线程：对应 OtaNode::execute_state_machine
  void executeStateMachine();
  // 100ms 周期任务线程：对应 OtaNode::execute_periodic_task
  void executePeriodicTask();

  // 对应 publisher->publish(msg)，由状态机内部调用
  void onPublishOtaStatus(const state_machine::OtaStateMsg& msg);
  // 对应 utils::ros2::callTTS
  void onSpeak(const std::string& text);

  std::string config_path_;
  config::Config config_;

  std::atomic_bool started_{false};
  std::thread state_machine_thread_;
  std::thread periodic_task_thread_;

  mutable std::mutex mutex_status_;
  state_machine::OtaStateMsg last_status_;
};

}  // namespace ota_mini
