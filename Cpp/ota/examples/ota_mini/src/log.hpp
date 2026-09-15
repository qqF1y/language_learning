/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/log.hpp
 * @Brief: 迷你日志 —— 对应真实工程的 MJR_INFO / MJR_ERROR（mjrrt 封装 spdlog）
 *
 * 语法点：
 *   变参模板 + 折叠表达式 + 宏 __VA_ARGS__ + function-local static mutex
 ***************************************************************/
#pragma once

#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace ota_mini {

// ---- 任意可流输出类型 -> std::string ----
template <typename T>
std::string toStr(const T& value) {
  std::ostringstream oss;
  oss << value;
  return oss.str();
}
// 非模板重载优先于模板（重载决议：同为精确匹配时非模板胜出）
inline std::string toStr(bool value) { return value ? "true" : "false"; }
inline std::string toStr(std::string_view value) { return std::string(value); }
inline std::string toStr(const char* value) { return value ? std::string(value) : "(null)"; }

// ⚠️ 易踩的坑：int8_t / uint8_t 本质是 signed char / unsigned char，
//    直接用 << 输出会当成**字符**打印（比如 6 会变成控制字符，看起来是空白）。
//    所以这里显式加两个重载，把它们按数字打出来。
//    （真实工程里用 fmt/{} 就能直接打印 int8_t，因为 fmt 内部按整型处理）
inline std::string toStr(signed char value) { return std::to_string(static_cast<int>(value)); }
inline std::string toStr(unsigned char value) { return std::to_string(static_cast<int>(value)); }

inline void replaceNextPlaceholder(std::string& out, std::size_t& cursor, const std::string& value) {
  const auto pos = out.find("{}", cursor);
  if (pos == std::string::npos) {
    return;
  }
  out.replace(pos, 2, value);
  cursor = pos + value.size();
}

// 对应 fmt::format("...{}...", a, b)
template <typename... Args>
std::string format(std::string_view pattern, const Args&... args) {
  std::string out{pattern};
  std::size_t cursor = 0;
  (replaceNextPlaceholder(out, cursor, toStr(args)), ...);   // 逗号折叠
  return out;
}
inline std::string format(std::string_view pattern) { return std::string(pattern); }

enum class LogLevel { Trace, Debug, Info, Warn, Error };

inline const char* toString(LogLevel level) {
  switch (level) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO ";
    case LogLevel::Warn:  return "WARN ";
    case LogLevel::Error: return "ERROR";
  }
  return "?????";
}

inline std::mutex& logMutex() {
  static std::mutex mtx;
  return mtx;
}

inline LogLevel& globalLogLevel() {
  static LogLevel level = LogLevel::Info;
  return level;
}

template <typename... Args>
void logMessage(LogLevel level, std::string_view pattern, const Args&... args) {
  if (static_cast<int>(level) < static_cast<int>(globalLogLevel())) {
    return;
  }
  const std::string line = format(pattern, args...);
  std::lock_guard<std::mutex> lock(logMutex());   // 多线程打印不串行
  std::cout << "[" << toString(level) << "] " << line << '\n';
  std::cout.flush();
}

}  // namespace ota_mini

#define OTA_LOG_TRACE(...) ::ota_mini::logMessage(::ota_mini::LogLevel::Trace, __VA_ARGS__)
#define OTA_LOG_DEBUG(...) ::ota_mini::logMessage(::ota_mini::LogLevel::Debug, __VA_ARGS__)
#define OTA_LOG_INFO(...)  ::ota_mini::logMessage(::ota_mini::LogLevel::Info, __VA_ARGS__)
#define OTA_LOG_WARN(...)  ::ota_mini::logMessage(::ota_mini::LogLevel::Warn, __VA_ARGS__)
#define OTA_LOG_ERROR(...) ::ota_mini::logMessage(::ota_mini::LogLevel::Error, __VA_ARGS__)
