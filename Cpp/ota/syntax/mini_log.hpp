/*************************************************************
 * @FilePath: /Cpp/ota/syntax/mini_log.hpp
 * @Brief: 迷你日志/格式化工具 —— 用于所有例程的公共输出
 *
 * 为什么需要它？
 *   真实工程用的是 fmt + spdlog(mjrrt)，本机没装，所以这里用
 *   **纯标准库**手写一个等价物，顺便把下面几个语法点演示清楚：
 *     1. 变参模板 (variadic template)
 *     2. 折叠表达式 (fold expression，C++17)
 *     3. 函数模板重载 + 模板特化的优先级
 *     4. 宏包装可变参数 (__VA_ARGS__)
 *     5. std::string_view 零拷贝入参
 *     6. function-local static + std::once 语义（C++11 起静态局部变量初始化线程安全）
 ***************************************************************/

#pragma once

#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace mini {

// ---------------------------------------------------------------------------
// 1) 任意"可流输出"类型 -> std::string
// ---------------------------------------------------------------------------
template <typename T>
std::string toStr(const T& value) {
  std::ostringstream oss;
  oss << value;  // 要求 T 支持 operator<<
  return oss.str();
}

// 非模板重载会优先于模板被选中（转换序列同为"精确匹配"时，非模板胜出）
inline std::string toStr(bool value) { return value ? "true" : "false"; }
inline std::string toStr(std::string_view value) { return std::string(value); }
inline std::string toStr(const char* value) { return value ? std::string(value) : "(null)"; }

// ⚠️ int8_t / uint8_t 本质是 signed char / unsigned char，
//    直接 << 会按**字符**输出（数字 6 会变成不可见控制字符）
inline std::string toStr(signed char value) { return std::to_string(static_cast<int>(value)); }
inline std::string toStr(unsigned char value) { return std::to_string(static_cast<int>(value)); }

// ---------------------------------------------------------------------------
// 2) 把 pattern 里第一个 "{}" 替换成 value，并把搜索起点右移
// ---------------------------------------------------------------------------
inline void replaceFirstPlaceholder(std::string& out, std::size_t& pos, const std::string& value) {
  const auto p = out.find("{}", pos);
  if (p == std::string::npos) {
    return;  // 占位符不够，多余的实参直接忽略（fmt 会抛异常，这里从简）
  }
  out.replace(p, 2, value);
  pos = p + value.size();
}

// ---------------------------------------------------------------------------
// 3) format("...{}...{}", a, b)：变参模板 + 逗号折叠表达式
//    (expr, ...) 会把每个实参对应的表达式从左到右依次展开求值
// ---------------------------------------------------------------------------
template <typename... Args>
std::string format(std::string_view pattern, const Args&... args) {
  std::string out{pattern};
  std::size_t pos = 0;
  (replaceFirstPlaceholder(out, pos, toStr(args)), ...);
  return out;
}

// 无参版本
inline std::string format(std::string_view pattern) { return std::string(pattern); }

// ---------------------------------------------------------------------------
// 4) 日志：等级 + 互斥锁 + 换行
// ---------------------------------------------------------------------------
enum class Level { Trace, Debug, Info, Warn, Error, Fatal };

inline const char* toString(Level level) {
  switch (level) {
    case Level::Trace: return "TRACE";
    case Level::Debug: return "DEBUG";
    case Level::Info:  return "INFO ";
    case Level::Warn:  return "WARN ";
    case Level::Error: return "ERROR";
    case Level::Fatal: return "FATAL";
  }
  return "?????";
}

// 函数内静态变量：整个进程唯一，且 C++11 起初始化是线程安全的
inline std::mutex& logMutex() {
  static std::mutex mtx;
  return mtx;
}

template <typename... Args>
void log(Level level, std::string_view pattern, const Args&... args) {
  const std::string line = format(pattern, args...);
  std::lock_guard<std::mutex> lock(logMutex());  // RAII：离开作用域自动解锁
  std::cout << "[" << toString(level) << "] " << line << '\n';
  std::cout.flush();
}

}  // namespace mini

// ---------------------------------------------------------------------------
// 5) 宏包装：对应工程里的 MJR_INFO / MJR_ERROR 等
//    宏的 __VA_ARGS__ 会把 "格式串 + 若干实参" 原样转发给 mini::log
// ---------------------------------------------------------------------------
#define MINI_TRACE(...) ::mini::log(::mini::Level::Trace, __VA_ARGS__)
#define MINI_DEBUG(...) ::mini::log(::mini::Level::Debug, __VA_ARGS__)
#define MINI_INFO(...)  ::mini::log(::mini::Level::Info, __VA_ARGS__)
#define MINI_WARN(...)  ::mini::log(::mini::Level::Warn, __VA_ARGS__)
#define MINI_ERROR(...) ::mini::log(::mini::Level::Error, __VA_ARGS__)

// 例程标题打印，方便看输出
inline void printTitle(const std::string& title) {
  std::cout << "\n========== " << title << " ==========\n";
}
inline void printStep(const std::string& step) {
  std::cout << "---- " << step << " ----\n";
}

// 每个例程开头统一调用，提示"对应真实工程的哪个语法点"
inline void printSourceHint(const std::string& source) {
  std::cout << "(真实工程出处: " << source << ")\n";
}
