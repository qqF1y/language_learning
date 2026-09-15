/*************************************************************
 * @FilePath: /Cpp/ota/syntax/08_optional_expected.cpp
 * @Brief: 语法点 08 —— std::optional 与 tl::expected 风格错误处理
 *
 * 真实工程出处：
 *   - src/ota/state_machine/parsing_img.cpp
 *       std::optional<TaskArtifact> getTaskArtifact(ota::task::TaskType task_type);
 *       auto artifact = getTaskArtifact(task_type);
 *       if (!artifact.has_value()) return false;
 *       ... artifact->dir ...
 *   - src/utils/filesystem/file_operator.hpp
 *       tl::expected<std::string, ErrorCode> readFile(std::string_view path);
 *       tl::expected<std::string, ErrorCode> read();
 *       return tl::unexpected(ErrorCode::FILE_NOT_OPENED);
 *   - 调用方：
 *       auto content = FileOperator::readFile(path);
 *       if (!content.has_value()) { return StateMachineType::INIT; }
 *       ... content.value() ...
 *
 * 为什么不用异常？
 *   嵌入式/实时场景里，异常有开销且难以预测；用返回值表达失败更可控。
 *   std::optional 只表达"有没有"，tl::expected 还能带"为什么失败"。
 ***************************************************************/

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "mini_log.hpp"
#include "xmacro_list.hpp"

// ===========================================================================
// 1) 生成 ErrorCode 枚举 + toString（复用例程 02 的 X-Macro 套路）
// ===========================================================================
namespace ota::utils {

#ifdef X
  #undef X
#endif

#define X(name, desc, value) name = value,
enum class ErrorCode {
  OTA_ERROR_CODE_LIST
};
#undef X

#define X(name, desc, value) {ErrorCode::name, desc},
static const std::pair<ErrorCode, const char*> kErrTable[] = {
    OTA_ERROR_CODE_LIST};
#undef X

const char* toString(ErrorCode code) {
  for (const auto& [c, desc] : kErrTable) {  // ← 结构化绑定
    if (c == code) return desc;
  }
  return "unknown";
}

// ===========================================================================
// 2) 手写迷你 Expected<T, E>，模仿 tl::expected 的接口
//    真实工程用的是 tl::expected（第三方单头文件库），接口完全一样
// ===========================================================================
template <typename E>
class Unexpected {
 public:
  explicit Unexpected(E e) : error_(std::move(e)) {}
  const E& error() const& { return error_; }
  E& error() & { return error_; }

 private:
  E error_;
};

template <typename E>
Unexpected<E> unexpected(E e) {
  return Unexpected<E>(std::move(e));
}

template <typename T, typename E>
class Expected {
 public:
  // 成功构造
  Expected(T value) : value_(std::move(value)), has_value_(true) {}          // NOLINT
  // 失败构造：Unexpected 是个"标签类型"，用来区分两个构造函数
  Expected(Unexpected<E> unex) : error_(std::move(unex.error())), has_value_(false) {}

  bool has_value() const { return has_value_; }
  explicit operator bool() const { return has_value_; }

  // value()：成功时取值；失败时抛异常（真实 tl::expected 也支持这种用法）
  const T& value() const& {
    if (!has_value_) {
      throw std::logic_error("bad expected access: no value");
    }
    return value_;
  }
  T& value() & { return const_cast<T&>(static_cast<const Expected*>(this)->value()); }

  // 失败时取值（真实工程最常用的就是这两个）
  const E& error() const& {
    if (has_value_) {
      throw std::logic_error("bad expected access: has value");
    }
    return error_;
  }

  // 常用糖：失败时给个默认值
  T value_or(T default_value) const {
    return has_value_ ? value_ : std::move(default_value);
  }

  // 等价于 std::optional 风格的解引用
  const T& operator*() const { return value(); }
  const T* operator->() const { return &value(); }

  // 链式：成功时继续处理，失败时原样透传错误（对应 tl::expected 的 and_then）
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

// ===========================================================================
// 3) 模拟 FileOperator：返回 Expected
// ===========================================================================
class FileOperator {
 public:
  static Expected<std::string, ErrorCode> readFile(const std::string& path) {
    if (path == "/no/such/file") {
      return unexpected(ErrorCode::NOT_FOUND);
    }
    if (path == "/permission/denied") {
      return unexpected(ErrorCode::READ_FAILED);
    }
    if (path.empty()) {
      return unexpected(ErrorCode::FILE_NOT_OPENED);
    }
    return std::string("42");  // 模拟读到状态机编号
  }
};

}  // namespace ota::utils

// ===========================================================================
// 4) 对应 parsing_img.cpp 的 std::optional 用法
// ===========================================================================
namespace ota::ota::state_machine {

struct TaskArtifact {
  std::string dir;
  std::string extension;
};

enum class TaskType { NX_SERIAL, NX_SELF, RK_ETHERCAT, UNKNOWN };

std::optional<TaskArtifact> getTaskArtifact(TaskType task_type) {
  switch (task_type) {
    case TaskType::NX_SERIAL:  return TaskArtifact{"/img/nx_mcu", ".bin"};
    case TaskType::NX_SELF:    return TaskArtifact{"/img/nx_self", ".deb"};
    case TaskType::RK_ETHERCAT:return TaskArtifact{"/img/rk_motor", ".bin"};
    case TaskType::UNKNOWN:    return std::nullopt;  // 明确表示"没有"
  }
  return std::nullopt;
}

// 调用方：has_value / operator-> / value_or 三种风格
std::string describeDir(TaskType t) {
  const auto artifact = getTaskArtifact(t);
  if (!artifact.has_value()) {
    return "(无对应升级文件)";
  }
  return artifact->dir + "  后缀=" + artifact->extension;
}

}  // namespace ota::ota::state_machine

int main() {
  using namespace ota::utils;
  using namespace ota::ota::state_machine;
  printTitle("08 std::optional 与 tl::expected 风格错误处理");
  printSourceHint("src/ota/state_machine/parsing_img.cpp, src/utils/filesystem/file_operator.hpp");

  printStep("1) std::optional：只关心\"有没有\"");
  MINI_INFO("NX_SERIAL   -> {}", describeDir(TaskType::NX_SERIAL));
  MINI_INFO("RK_ETHERCAT -> {}", describeDir(TaskType::RK_ETHERCAT));
  MINI_INFO("UNKNOWN     -> {}", describeDir(TaskType::UNKNOWN));

  const auto none = getTaskArtifact(TaskType::UNKNOWN);
  MINI_INFO("nullopt 判断: has_value={}, 直接 value() 会抛异常", none.has_value());
  MINI_INFO("value_or 兜底: {}/{}", none.value_or(TaskArtifact{"(default)", "-"}).dir,
            none.value_or(TaskArtifact{"(default)", "-"}).extension);

  printStep("2) Expected：失败时还能带上原因");
  for (const std::string& path : {std::string("/home/eame/.../current_ota_state"), std::string("/no/such/file"),
                                  std::string("/permission/denied"), std::string("")}) {
    const auto result = FileOperator::readFile(path);
    if (result.has_value()) {
      MINI_INFO("读取 [{}] 成功, 内容 = {}", path, result.value());
    } else {
      MINI_ERROR("读取 [{}] 失败, 原因 = {} ({})", path, toString(result.error()),
                 static_cast<int>(result.error()));
    }
  }

  printStep("3) 和异常风格的对比");
  try {
    (void)FileOperator::readFile("/no/such/file").value();   // 故意在失败结果上取 value
  } catch (const std::logic_error& e) {
    MINI_WARN("value() 在失败时抛异常: {}", e.what());
  }

  printStep("4) and_then 链式组合（成功才继续，失败自动透传错误）");
  const auto chain = FileOperator::readFile("/home/eame/.../current_ota_state")
                         .and_then([](const std::string& content) -> Expected<int, ErrorCode> {
                           if (content.empty()) {
                             return unexpected(ErrorCode::READ_FAILED);
                           }
                           return std::stoi(content);   // "42" -> 42
                         });
  MINI_INFO("链式成功: has_value={}, 状态机编号={}", chain.has_value(), chain.value_or(-999));

  const auto chain_fail = FileOperator::readFile("/no/such/file")
                              .and_then([](const std::string& content) -> Expected<int, ErrorCode> {
                                return std::stoi(content);
                              });
  MINI_INFO("链式失败: has_value={}, 错误透传={} ", chain_fail.has_value(), toString(chain_fail.error()));

  printStep("小结");
  std::cout << R"(
    - std::optional<T>          : 有/没有 两种状态，nullopt 表示"没有"
    - Expected<T,E> (=tl::expected): 成功给 T，失败给 E（错误码/错误信息）
    - 工厂函数常用 optional；带错误原因的 IO/网络/解析用 Expected
    - 惯用法：
        if (!r) return r.error();     // 失败立刻透传
        auto v = r.value();           // 成功取值
      （真实工程大量存在 if (!x.has_value()) { ...return; } ）
    - 注意：optional 的 value() 在 nullopt 时抛 bad_optional_access；
        Expected 的 value() 在失败时也有对应异常（或 UB，看实现）
    - and_then / transform 链式可以少写很多 if，但别链太长影响可读性
)";
  return 0;
}
