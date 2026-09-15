/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/ota_node.cpp
 * @Brief: 节点装配层实现
 ***************************************************************/

#include "ota_node.hpp"

#include <chrono>
#include <thread>

#include "log.hpp"
#include "states.hpp"

namespace ota_mini {

using state_machine::StateMachineData;
using state_machine::StateMachineFunction;
using state_machine::StateMachineType;

OtaNode::OtaNode(std::string config_path) : config_path_(std::move(config_path)) {}

OtaNode::~OtaNode() { stop(); }

bool OtaNode::start() {
  if (started_.load()) {
    return true;
  }

  // ---- 1) 加载配置 ----
  auto loaded = config::loadFromYaml(config_path_);
  if (!loaded.has_value()) {
    OTA_LOG_ERROR("加载配置失败: {} ({})", config_path_, toString(loaded.error()));
    return false;
  }
  config_ = loaded.value();

  auto& data = StateMachineFunction::data();
  data.configures = config_;
  data.paths.work_dir = config_.work_dir;

  // ---- 2) 挂上"发布器"和"语音客户端" ----
  // 真实工程这里是 node->create_publisher<OtaState>("/ota_status") 与 create_client<SetTTS>()
  data.publish_status = [this](const state_machine::OtaStateMsg& msg) { onPublishOtaStatus(msg); };
  data.speak = [this](const std::string& text) { onSpeak(text); };

  // ---- 3) 打印状态机图（对应 MJR_INFO("state machine plant uml: \n{}", ...)） ----
  OTA_LOG_INFO("状态机转移表:\n{}", state_machine::getStateMachinePlantUml());

  // ---- 4) 起线程 ----
  data.running.store(true);
  started_.store(true);
  state_machine_thread_ = std::thread([this] { executeStateMachine(); });
  periodic_task_thread_ = std::thread([this] { executePeriodicTask(); });

  OTA_LOG_INFO("OtaNode 已启动 (config={})", config_path_);
  return true;
}

void OtaNode::stop() {
  if (!started_.load()) {
    return;
  }
  auto& data = StateMachineFunction::data();
  data.running.store(false);
  data.idle_task_waiter.notify();      // 唤醒睡在 IDLE 的状态机线程
  data.failed_task_waiter.notify();    // 唤醒睡在 FAILED 的状态机线程

  if (state_machine_thread_.joinable()) {
    state_machine_thread_.join();
  }
  if (periodic_task_thread_.joinable()) {
    periodic_task_thread_.join();
  }
  started_.store(false);
  OTA_LOG_INFO("OtaNode 已停止");
}

// ===========================================================================
// 状态机线程
// ===========================================================================
void OtaNode::executeStateMachine() {
  auto& data = StateMachineFunction::data();

  while (data.running.load()) {
    const auto current = data.state_machine_type.load();
    try {
      // 对应 ota.cpp 的 state_machine_list.at(current)();
      //   —— 每个状态"用完即毁"，靠基类析构推进到下一状态
      auto state = state_machine::createState(current);
      if (!state) {
        OTA_LOG_ERROR("没有为状态 {} 注册处理函数", toString(current));
        std::terminate();
      }
      state->run();
    } catch (const std::exception& e) {
      // 真实工程：catch 到异常就 std::terminate()，不带病继续
      OTA_LOG_ERROR("状态 {} 执行异常: {}", toString(current), e.what());
      std::terminate();
    }
  }
}

// ===========================================================================
// 周期任务线程（100ms）
// ===========================================================================
void OtaNode::executePeriodicTask() {
  auto& data = StateMachineFunction::data();
  int tick = 0;

  while (data.running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ++tick;
    if (tick % 10 == 0) {   // 每 1 秒打一次，避免刷屏
      OTA_LOG_TRACE("[周期任务] state={}, battery={}%, task={}/{}",
                    toString(data.state_machine_type.load()), data.battery_percentage.load(),
                    data.current_task_index.load(), data.task_type_list.size());
    }
  }
}

// ===========================================================================
// Service: /ota_local
// ===========================================================================
bool OtaNode::srvOtaLocalCmd(const std::string& data_str, std::string* err) {
  OTA_LOG_INFO("[service /ota_local] data = {}", data_str);

  auto& data = StateMachineFunction::data();

  // 电量检查（真实工程也在这里挡一道）
  if (data.battery_percentage.load() < config_.battery_limit) {
    if (err) {
      *err = "电量不足";
    }
    OTA_LOG_ERROR("拒绝升级：电量 {}% 低于阈值 {}%", data.battery_percentage.load(), config_.battery_limit);
    return false;
  }

  state_machine::UpgradeType type = state_machine::UpgradeType::UNKNOWN;
  if (data_str == "ALL") {
    type = state_machine::UpgradeType::LOCAL_ALL;
  } else if (data_str == "RK_MOTOR") {
    type = state_machine::UpgradeType::LOCAL_RK_MOTOR;
  } else if (data_str == "RK_MCU") {
    type = state_machine::UpgradeType::LOCAL_RK_MCU;
  } else if (data_str == "RK_SELF") {
    type = state_machine::UpgradeType::LOCAL_RK_SELF;
  } else if (data_str == "NX_SELF") {
    type = state_machine::UpgradeType::LOCAL_NX_SELF;
  } else if (data_str.size() > 4 && data_str.substr(data_str.size() - 4) == ".bin") {
    // 对应"升级 NX 直连的单个 MCU 文件"
    type = state_machine::UpgradeType::LOCAL_RK_MCU;
  } else {
    if (err) {
      *err = "无法识别的升级参数";
    }
    OTA_LOG_ERROR("无法识别的 /ota_local 参数: {}", data_str);
    return false;
  }

  const bool accepted = data.setIsReceiveTask(type);
  if (!accepted) {
    if (err) {
      *err = "当前状态不允许接收任务";
    }
    return false;
  }
  return true;
}

// ===========================================================================
// Service: /ota_eame
// ===========================================================================
bool OtaNode::srvOtaCloudCmd(const std::string& version, const std::string& url, std::string* err) {
  OTA_LOG_INFO("[service /ota_eame] version = {}, url = {}", version, url);

  auto& data = StateMachineFunction::data();

  if (data.battery_percentage.load() < config_.battery_limit) {
    if (err) {
      *err = "电量不足";
    }
    return false;
  }
  if (url.empty()) {
    if (err) {
      *err = "url 为空";
    }
    return false;
  }

  if (!data.setIsReceiveTask(state_machine::UpgradeType::CLOUD, url, version)) {
    if (err) {
      *err = "当前状态不允许接收任务";
    }
    return false;
  }
  return true;
}

// ===========================================================================
// Service: /ota_reset
// ===========================================================================
bool OtaNode::srvOtaReset(std::string* err) {
  OTA_LOG_INFO("[service /ota_reset]");
  auto& data = StateMachineFunction::data();
  if (!data.setIsFailedReset()) {
    if (err) {
      *err = "当前状态不是 FAILED";
    }
    return false;
  }
  return true;
}

// ===========================================================================
// Topic 输入
// ===========================================================================
void OtaNode::subBatteryState(int percentage) {
  // 对应真实工程订阅回调里的"值没变就不打印"
  auto& data = StateMachineFunction::data();
  if (data.battery_percentage.load() == percentage) {
    return;
  }
  data.battery_percentage.store(percentage);
  OTA_LOG_INFO("[topic /battery_state] 电量更新为 {}%", percentage);
}

void OtaNode::subIsHangUp(bool hang_up) {
  auto& data = StateMachineFunction::data();
  if (data.is_hang_up.load() == hang_up) {
    return;
  }
  data.is_hang_up.store(hang_up);
  OTA_LOG_INFO("[topic /manager/robot_hanged] 挂起状态 = {}", hang_up ? "已挂起" : "未挂起");
}

// ===========================================================================
// 发布 /ota_status
// ===========================================================================
void OtaNode::onPublishOtaStatus(const state_machine::OtaStateMsg& msg) {
  {
    std::lock_guard<std::mutex> lock(mutex_status_);
    last_status_ = msg;          // 存副本，供外部查询
  }
  OTA_LOG_INFO("[topic /ota_status] state={} ({}), 进度 {}/{}, 下载 {}%, 说明: {}",
               msg.state, state_machine::toString(StateMachineFunction::data().state_machine_type.load()),
               msg.pkg_current, msg.pkg_total, msg.download_progress,
               msg.state_details.empty() ? "-" : msg.state_details);
}

void OtaNode::onSpeak(const std::string& text) {
  // 对应 utils::ros2::callTTS(client_set_tts, 1, "audio_id:xxx")
  OTA_LOG_INFO("[TTS] 语音播报: {}", text);
}

state_machine::OtaStateMsg OtaNode::lastOtaStatus() const {
  std::lock_guard<std::mutex> lock(mutex_status_);
  return last_status_;
}

StateMachineType OtaNode::currentState() const {
  return StateMachineFunction::data().state_machine_type.load();
}

}  // namespace ota_mini
