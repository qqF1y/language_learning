/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/config.cpp
 * @Brief: 极简 yaml 子集解析实现
 *
 * 支持的语法（够本例程用）：
 *   key: 123              # 标量：int
 *   key: true / false     # 标量：bool
 *   key: "字符串"          # 标量：string（引号可选）
 *   key:                  # 列表：后面跟若干 "- item"
 *     - item1
 *     - item2
 *   # 注释行
 ***************************************************************/

#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "log.hpp"

namespace ota_mini::config {
namespace {

std::string trim(std::string_view s) {
  const auto begin = s.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) {
    return "";
  }
  const auto end = s.find_last_not_of(" \t\r\n");
  return std::string(s.substr(begin, end - begin + 1));
}

std::string stripQuotes(std::string s) {
  if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
    return s.substr(1, s.size() - 2);
  }
  return s;
}

bool parseBool(const std::string& s, bool& out) {
  std::string lower = s;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (lower == "true" || lower == "yes" || lower == "on") {
    out = true;
    return true;
  }
  if (lower == "false" || lower == "no" || lower == "off") {
    out = false;
    return true;
  }
  return false;
}

}  // namespace

Expected<Config, ErrorCode> loadFromYaml(const std::string& yaml_path) {
  std::ifstream ifs(yaml_path);
  if (!ifs.is_open()) {
    OTA_LOG_ERROR("打不开配置文件: {}", yaml_path);
    return unexpected(ErrorCode::NOT_FOUND);
  }

  Config cfg;

  // 解析中间结果
  std::unordered_map<std::string, std::string> scalars;
  std::unordered_map<std::string, std::vector<std::string>> lists;
  std::string current_list_key;

  std::string raw_line;
  while (std::getline(ifs, raw_line)) {
    // 去掉注释（简单处理：不处理引号内 # 的情况）
    if (const auto comment = raw_line.find('#'); comment != std::string::npos) {
      raw_line = raw_line.substr(0, comment);
    }
    const std::string line = trim(raw_line);
    if (line.empty()) {
      continue;
    }

    if (line.rfind("- ", 0) == 0) {                    // 列表项
      if (!current_list_key.empty()) {
        lists[current_list_key].push_back(stripQuotes(trim(line.substr(2))));
      }
      continue;
    }

    if (const auto colon = line.find(':'); colon != std::string::npos) {   // if 初始化语句
      const std::string key = trim(line.substr(0, colon));
      const std::string value = trim(line.substr(colon + 1));
      if (value.empty()) {
        current_list_key = key;      // 后面跟列表
        lists[key];                  // 确保 key 存在
      } else {
        scalars[key] = stripQuotes(value);
        current_list_key.clear();
      }
      continue;
    }

    OTA_LOG_WARN("忽略无法解析的配置行: {}", line);
  }

  // ---- 标量映射到结构体字段 ----
  auto getInt = [&](const std::string& key, int& out) {
    if (const auto it = scalars.find(key); it != scalars.end()) {
      out = std::atoi(it->second.c_str());
    }
  };
  auto getBool = [&](const std::string& key, bool& out) {
    if (const auto it = scalars.find(key); it != scalars.end()) {
      if (!parseBool(it->second, out)) {
        OTA_LOG_WARN("配置项 {} 不是合法布尔值: {}", key, it->second);
      }
    }
  };
  auto getStr = [&](const std::string& key, std::string& out) {
    if (const auto it = scalars.find(key); it != scalars.end()) {
      out = it->second;
    }
  };

  getInt("thread_pool_size", cfg.thread_pool_size);
  getInt("battery_limit", cfg.battery_limit);
  getBool("reboot_after_ota", cfg.reboot_after_ota);
  getBool("hard_reboot", cfg.hard_reboot);
  getBool("prepare_befor_ota", cfg.prepare_befor_ota);
  getInt("prepare_befor_ota_time", cfg.prepare_befor_ota_time);
  getStr("work_dir", cfg.work_dir);

  if (const auto it = lists.find("upgrade_sequence"); it != lists.end()) {
    cfg.upgrade_sequence = it->second;
  }
  if (cfg.upgrade_sequence.empty()) {
    OTA_LOG_WARN("upgrade_sequence 为空，使用默认序列");
    cfg.upgrade_sequence = {"nx_serial", "rk_ethercat", "rk_self", "nx_self"};
  }

  // ---- device_ports + device_fail_ids -> std::vector<Device> ----
  if (const auto it = lists.find("device_ports"); it != lists.end()) {
    std::vector<int> fail_ids;
    if (const auto fit = lists.find("device_fail_ids"); fit != lists.end()) {
      for (const auto& s : fit->second) {
        fail_ids.push_back(std::atoi(s.c_str()));
      }
    }

    int index = 0;
    for (const auto& port : it->second) {
      Device d;
      d.name = "dev_" + std::to_string(index);
      d.port = port;
      d.baud_rate = 115200 + index * 1000;
      d.should_fail = std::find(fail_ids.begin(), fail_ids.end(), index) != fail_ids.end();
      cfg.devices.push_back(std::move(d));
      ++index;
    }
  }

  return cfg;
}

std::string describe(const Config& cfg) {
  std::ostringstream oss;
  oss << "\n  thread_pool_size    : " << cfg.thread_pool_size
      << "\n  battery_limit       : " << cfg.battery_limit
      << "\n  reboot_after_ota    : " << (cfg.reboot_after_ota ? "true" : "false")
      << "\n  hard_reboot         : " << (cfg.hard_reboot ? "true" : "false")
      << "\n  prepare_befor_ota   : " << (cfg.prepare_befor_ota ? "true" : "false")
      << "\n  prepare_befor_ota_t : " << cfg.prepare_befor_ota_time
      << "\n  work_dir            : " << cfg.work_dir
      << "\n  upgrade_sequence    : ";
  for (const auto& s : cfg.upgrade_sequence) {
    oss << s << " ";
  }
  oss << "\n  devices             : ";
  for (const auto& d : cfg.devices) {
    oss << d.name << "(" << d.port << (d.should_fail ? ",会失败" : "") << ") ";
  }
  return oss.str();
}

}  // namespace ota_mini::config
