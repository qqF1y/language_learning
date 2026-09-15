/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/result.hpp
 * @Brief: ErrorCode（X-Macro 生成）+ Expected<T,E>（仿 tl::expected）
 *
 * 对应真实工程：
 *   - src/utils/waiter/error.hpp        OTA_WAITER_ERROR_CODE_LIST
 *   - src/utils/filesystem/error.hpp    各种 ErrorCode
 *   - tl::expected<std::string, ErrorCode> FileOperator::readFile(...)
 ***************************************************************/
#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace ota_mini {

// ===========================================================================
// 错误码：X-Macro 单一数据源
// ===========================================================================
#define OTA_MINI_ERROR_LIST         \
  X(OK, "成功", 0)                  \
  X(FAILED, "通用失败", 1)          \
  X(TIMEOUT, "超时", 2)             \
  X(NOT_FOUND, "文件或目录不存在", 3) \
  X(READ_FAILED, "读取失败", 4)     \
  X(WRITE_FAILED, "写入失败", 5)    \
  X(INVALID_ARG, "参数非法", 6)     \
  X(NO_TASK, "没有可执行的升级任务", 7) \
  X(LOW_BATTERY, "电量不足", 8)     \
  X(NOT_IDLE, "当前非空闲状态", 9)  \
  X(ILLEGAL_TRANSITION, "非法状态转移", 10)

#ifdef X
  #undef X
#endif

#define X(name, desc, value) name = value,
enum class ErrorCode {
  OTA_MINI_ERROR_LIST
};
#undef X

#define X(name, desc, value) {ErrorCode::name, desc},
static const std::pair<ErrorCode, const char*> kErrorTable[] = {
    OTA_MINI_ERROR_LIST};
#undef X

inline const char* toString(ErrorCode code) {
  for (const auto& [c, desc] : kErrorTable) {   // 结构化绑定
    if (c == code) {
      return desc;
    }
  }
  return "unknown error";
}

// ===========================================================================
// Expected<T, E>：成功给 T，失败给 E
// ===========================================================================
template <typename E>
class Unexpected {
 public:
  explicit Unexpected(E e) : error_(std::move(e)) {}
  const E& error() const { return error_; }

 private:
  E error_;
};

template <typename E>
Unexpected<E> unexpected(E e) {
  return Unexpected<E>(std::move(e));
}

// 只有错误、没有值的 Expected（对应 tl::expected<void, E>）
template <typename E>
class ExpectedVoid {
 public:
  ExpectedVoid() : has_value_(true) {}
  ExpectedVoid(Unexpected<E> unex) : error_(std::move(unex.error())), has_value_(false) {}  // NOLINT

  bool has_value() const { return has_value_; }
  explicit operator bool() const { return has_value_; }

  const E& error() const { return error_; }

 private:
  E error_{};
  bool has_value_ = false;
};

template <typename T, typename E>
class Expected {
 public:
  Expected(T value) : value_(std::move(value)), has_value_(true) {}                // NOLINT
  Expected(Unexpected<E> unex) : error_(std::move(unex.error())), has_value_(false) {}  // NOLINT

  bool has_value() const { return has_value_; }
  explicit operator bool() const { return has_value_; }

  const T& value() const& {
    if (!has_value_) {
      throw std::logic_error("Expected: no value");
    }
    return value_;
  }

  const E& error() const& {
    if (has_value_) {
      throw std::logic_error("Expected: has value");
    }
    return error_;
  }

  T value_or(T fallback) const { return has_value_ ? value_ : std::move(fallback); }

  template <typename F>
  auto and_then(F&& f) const -> decltype(f(std::declval<const T&>())) {
    using R = decltype(f(std::declval<const T&>()));
    if (!has_value_) {
      return R(unexpected(error_));
    }
    return f(value_);
  }

 private:
  T value_{};
  E error_{};
  bool has_value_ = false;
};

// tl::expected 里也有这个糖：把 bool/无返回值函数的结果统一成 ExpectedVoid
inline ExpectedVoid<ErrorCode> ok() { return ExpectedVoid<ErrorCode>{}; }
inline ExpectedVoid<ErrorCode> fail(ErrorCode code) { return ExpectedVoid<ErrorCode>(unexpected(code)); }

}  // namespace ota_mini
