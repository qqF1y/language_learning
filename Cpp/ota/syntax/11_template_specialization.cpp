/*************************************************************
 * @FilePath: /Cpp/ota/syntax/11_template_specialization.cpp
 * @Brief: 语法点 11 —— 模板、全特化、偏特化、以及 fmt::formatter 特化套路
 *
 * 真实工程出处：
 *   - src/utils/configures/configures.hpp
 *       结构体 MotorType / SerialDevice / EtherCatConfig ...
 *       然后为每个结构体写了一个 fmt formatter 特化：
 *       template <> struct fmt::formatter<humanoid_ota::utils::config::MotorType>
 *                    : fmt::formatter<std::string> {
 *         auto format(const MotorType& data, format_context& ctx) const -> decltype(ctx.out()) {
 *           return fmt::format_to(ctx.out(), "master_id: {}, slave_id: {}, ...",
 *                                 data.master_id, data.slave_id, ...);
 *         }
 *       };
 *     -> 这样 MJR_INFO("configures: {}", configures) 就能直接打印整个结构体
 *   - src/utils/configures/configures.hpp 里还用了 reflect-cpp 的
 *       rfl::NamedTuple<rfl::Field<"master_id", int>, ...>
 *     把结构体字段映射成"名字 + 类型"，实现 yaml 自动解析（也是模板元编程）
 *
 * 本例用纯标准库复刻这两个套路。
 ***************************************************************/

#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "mini_log.hpp"

namespace ota::config {

// ===========================================================================
// 1) 普通结构体：OTA 配置
// ===========================================================================
struct MotorType {
  int master_id = 0;
  int slave_id = 0;
  int canfd_id = 0;
  std::vector<std::string> motor_type;
};

struct SerialDevice {
  std::string name;
  std::string serial_port;
  int baud_rate = 0;
  bool read_version = false;
};

// ===========================================================================
// 2) 类模板 + 偏特化：用"类型 -> 描述"演示模板选择规则
// ===========================================================================
template <typename T>
struct TypeName {
  static std::string get() { return "unknown type"; }     // 主模板
};

// 全特化：为具体类型量身定制
template <>
struct TypeName<int> {
  static std::string get() { return "int (32位有符号整数)"; }
};

template <>
struct TypeName<std::string> {
  static std::string get() { return "std::string"; }
};

// 偏特化：对"任意指针类型"都适用
template <typename T>
struct TypeName<T*> {
  static std::string get() { return "pointer to " + TypeName<T>::get(); }
};

// 偏特化：对"任意 vector"都适用
template <typename T>
struct TypeName<std::vector<T>> {
  static std::string get() { return "std::vector<" + TypeName<T>::get() + ">"; }
};

// ===========================================================================
// 3) 函数模板 + 重载决议 + 显式特化
// ===========================================================================
template <typename T>
std::string describe(const T& value) {
  std::ostringstream oss;
  oss << "泛型版本: " << value;
  return oss.str();
}

// 函数模板的"显式特化"（注意：不能用偏特化）
template <>
std::string describe<bool>(const bool& value) {
  return std::string("bool 特化版本: ") + (value ? "true" : "false");
}

// ===========================================================================
// 4) 复刻 fmt::formatter 套路：用 traits 类给结构体加"可打印"能力
//    真实工程是特化 fmt::formatter，这里特化自己的 mini::Formatter
//    思路完全一样：给每个类型提供一个 format(const T&) -> std::string
// ===========================================================================
template <typename T>
struct Formatter {
  // 主模板留空：没有特化的类型就不能被打印（编译期报错，很安全）
};

template <>
struct Formatter<MotorType> {
  static std::string format(const MotorType& m) {
    std::ostringstream oss;
    oss << "MotorType{master_id: " << m.master_id << ", slave_id: " << m.slave_id
        << ", canfd_id: " << m.canfd_id << ", motor_type: [";
    for (std::size_t i = 0; i < m.motor_type.size(); ++i) {
      oss << (i ? ", " : "") << m.motor_type[i];
    }
    oss << "]}";
    return oss.str();
  }
};

template <>
struct Formatter<SerialDevice> {
  static std::string format(const SerialDevice& d) {
    std::ostringstream oss;
    oss << "SerialDevice{name: " << d.name << ", port: " << d.serial_port
        << ", baud: " << d.baud_rate << ", read_version: " << (d.read_version ? "true" : "false") << "}";
    return oss.str();
  }
};

// 通用打印入口：直接调用 Formatter<T>::format
// 如果 T 没有对应特化，这里就是**编译错误**（而不是运行时崩溃）——
// 这正是模板元编程的价值：错误提前到编译期，且错误信息会指向这一行。
template <typename T>
std::string printable(const T& value) {
  return Formatter<T>::format(value);
}

// ===========================================================================
// 5) 变参模板 + 折叠表达式：打印任意多个可打印对象
// ===========================================================================
template <typename... Ts>
void printAll(const Ts&... values) {
  ((std::cout << "  - " << printable(values) << "\n"), ...);   // 逗号折叠
}

// ===========================================================================
// 6) 可变参数包的"类型个数"：sizeof...，以及 C++17 的折叠做全真判断
// ===========================================================================
template <typename... Ts>
constexpr std::size_t typeCount() {
  return sizeof...(Ts);
}

}  // namespace ota::config

int main() {
  using namespace ota::config;
  printTitle("11 模板 / 全特化 / 偏特化 / formatter 套路");
  printSourceHint("src/utils/configures/configures.hpp");

  printStep("1) 类模板特化选择：(类型 -> 描述)");
  MINI_INFO("TypeName<int>              : {}", TypeName<int>::get());
  MINI_INFO("TypeName<double>           : {}", TypeName<double>::get());
  MINI_INFO("TypeName<std::string>      : {}", TypeName<std::string>::get());
  MINI_INFO("TypeName<int*>             : {} (偏特化 T*)", TypeName<int*>::get());
  MINI_INFO("TypeName<std::vector<int>> : {} (偏特化 vector<T>)", TypeName<std::vector<int>>::get());
  MINI_INFO("TypeName<vector<vector<int>>>: {}", TypeName<std::vector<std::vector<int>>>::get());

  printStep("2) 函数模板：泛型 vs 显式特化");
  MINI_INFO("{}", describe(42));
  MINI_INFO("{}", describe(std::string("ota")));
  MINI_INFO("{}", describe(true));       // 走 bool 显式特化
  MINI_INFO("{}", describe(1 == 1));     // 同样走 bool 特化

  printStep("3) 结构体 -> 可打印（复刻 fmt::formatter 特化）");
  MotorType motor{1, 2, 0, {"dm8009", "dm4340"}};
  SerialDevice dev{"nx_mcu", "/dev/ttyTHS1", 921600, true};

  MINI_INFO("MotorType   : {}", printable(motor));
  MINI_INFO("SerialDevice: {}", printable(dev));

  printStep("4) 变参模板 + 折叠表达式：一次打印多个对象");
  printAll(motor, dev);

  printStep("5) sizeof...(Types)：编译期类型计数");
  MINI_INFO("typeCount<int, double, std::string>() = {}", typeCount<int, double, std::string>());
  MINI_INFO("typeCount<>() = {}", typeCount<>());

  printStep("6) 对应真实工程的 rfl::NamedTuple（字段名 + 类型 编译期映射）");
  // 真实工程写的是：
  //   using MotorType_reflection = rfl::NamedTuple<
  //       rfl::Field<"master_id", int>,
  //       rfl::Field<"slave_id", int>, ...>;
  // 这里用简化的"字段描述表"表达同样的思想：字段名 -> 取值 lambda
  struct Field {
    const char* name;
    std::string (*getter)(const MotorType&);
  };
  const Field fields[] = {
      {"master_id", [](const MotorType& m) { return std::to_string(m.master_id); }},
      {"slave_id", [](const MotorType& m) { return std::to_string(m.slave_id); }},
  };
  for (const auto& f : fields) {
    MINI_INFO("反射字段 {} = {}", f.name, f.getter(motor));
  }

  printStep("小结");
  std::cout << R"(
    - 类模板：主模板 / 全特化(template<> struct X<int>) / 偏特化(template<T> struct X<T*>)
    - 函数模板：只有全特化，没有偏特化；偏特化需求用重载或 if constexpr 解决
    - 重载决议的优先级：非模板函数 > 模板特化 > 主模板
    - if constexpr：编译期分支，未命中的分支不会实例化（可用来做"有/没有某成员"的探测）
    - fmt::formatter 特化 = 给自定义类型实现 format()，之后 {} 就能直接打印它
    - 折叠表达式 (expr, ...) / (... + expr) 是变参模板的标准遍历手段
    - 真实工程的 rfl::NamedTuple 用"类型"承载字段名，从而在编译期生成 yaml 解析代码
)";
  return 0;
}
