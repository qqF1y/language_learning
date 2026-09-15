/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/config.hpp
 * @Brief: 配置结构体 + 极简 yaml 解析
 *
 * 对应真实工程：
 *   - src/utils/configures/configures.hpp  struct Configures { ... }
 *   - 真实工程用 reflect-cpp (rfl::NamedTuple) 做反射式 yaml 解析，
 *     这里用最朴素的手写解析，但**结构体定义风格完全一致**：
 *     每个字段都有类内默认值（C++11 起支持）
 ***************************************************************/
#pragma once

#include <string>
#include <vector>

#include "result.hpp"

namespace ota_mini::config {

struct Device {
  std::string name;
  std::string port;
  int baud_rate = 115200;
  bool should_fail = false;
};

struct Config {
  // ---- 基本配置 ----
  int thread_pool_size = 4;
  int battery_limit = 30;
  bool reboot_after_ota = false;
  bool hard_reboot = false;
  bool prepare_befor_ota = false;
  int prepare_befor_ota_time = 5;
  std::string work_dir = "/tmp/ota_mini/work";

  // ---- 升级序列（对应 yaml 的 upgrade_sequence）----
  std::vector<std::string> upgrade_sequence;

  // ---- 设备 ----
  std::vector<Device> devices;
};

// 从 yaml 文件加载配置；失败返回 Unexpected<ErrorCode>
Expected<Config, ErrorCode> loadFromYaml(const std::string& yaml_path);

// 打印整个配置（对应真实工程里的 fmt::formatter<Configures> 特化）
std::string describe(const Config& cfg);

}  // namespace ota_mini::config
