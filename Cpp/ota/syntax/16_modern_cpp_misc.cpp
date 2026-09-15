/*************************************************************
 * @FilePath: /Cpp/ota/syntax/16_modern_cpp_misc.cpp
 * @Brief: 语法点 16 —— 现代 C++ 零散但高频的语法糖（C++17/20）
 *
 * 真实工程出处（每个语法点都在工程里出现过）：
 *   - 结构化绑定      : 08 例程里的 for (const auto& [c, desc] : table)
 *   - if 初始化语句   : parsing_img.cpp   if (auto artifact = ...; artifact.has_value())
 *   - if constexpr    : 模板里做编译期分支
 *   - [[likely]]      : state_machine.cpp   if (!is_success) [[unlikely]] { std::terminate(); }
 *   - 初始化捕获      : [n = count * 2] { ... }
 *   - std::variant    : 替代 union / 多态小对象
 *   - std::clamp      : 电池电量/进度裁剪
 *   - [[nodiscard]]   : 提醒必须检查返回值（比如 ErrorCode）
 *   - 指定初始化器    : configures 里 {.master_id = 0, .slave_id = 0}
 *   - [[fallthrough]] : switch 里故意不写 break 必须标注
 *   - auto 返回值推导 : 简写函数签名
 ***************************************************************/

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "mini_log.hpp"

using namespace std::string_view_literals;   // 让 "xxx"sv 可用

namespace ota {

// ===========================================================================
// 1) 指定初始化器（C++20）：只给关心的字段赋值，其余用默认值
//    真实工程 configures.hpp 的 MotorType 就是这种聚合结构体
// ===========================================================================
struct MotorType {
  int master_id = 0;
  int slave_id = 0;
  int canfd_id = 0;
  std::string protocol = "ethercat";
};

// ===========================================================================
// 2) [[nodiscard]]：返回值必须被使用，否则编译告警
//    真实工程大量 ErrorCode / tl::expected 返回值都应该这么标
// ===========================================================================
enum class ErrorCode { OK, FAILED };

[[nodiscard]] ErrorCode checkBattery(int percentage) {
  return percentage >= 30 ? ErrorCode::OK : ErrorCode::FAILED;
}

// ===========================================================================
// 3) if constexpr + auto 返回值推导 + trailing return type
// ===========================================================================
template <typename T>
auto describe(T value) {
  if constexpr (std::is_integral_v<T>) {
    return std::string("整数: ") + std::to_string(value);
  } else if constexpr (std::is_floating_point_v<T>) {
    return std::string("浮点: ") + std::to_string(value);
  } else {
    return std::string("其它: ") + std::string(value);
  }
}

// 显式尾置返回类型（C++11 风格，模板里推导不出来时仍需用）
template <typename T, typename U>
auto add(T a, U b) -> decltype(a + b) {
  return a + b;
}

// ===========================================================================
// 4) std::variant：类型安全的 union，适合表示"升级载荷可能是几种形态之一"
// ===========================================================================
using UpgradePayload = std::variant<std::monostate,     // 空（还没收到任务）
                                    std::string,        // 本地：文件名
                                    std::pair<std::string, std::string>>;  // 云端：{version, url}

// 用重载集合实现 std::visit 的"多态 lambda"（C++17 经典技巧，也叫 overloaded）
template <typename... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <typename... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;   // C++17 类模板参数推导(CTAD)

std::string describePayload(const UpgradePayload& payload) {
  return std::visit(
      Overloaded{
          [](std::monostate) { return std::string("(空) 还没有升级任务"); },
          [](const std::string& file) { return std::string("本地升级文件: ") + file; },
          [](const std::pair<std::string, std::string>& cloud) {
            return "云端升级: version=" + cloud.first + ", url=" + cloud.second;
          },
      },
      payload);
}

}  // namespace ota

int main() {
  using namespace ota;
  printTitle("16 现代 C++ 高频语法糖");
  printSourceHint("src/ota/state_machine/*.cpp, src/utils/configures/configures.hpp");

  printStep("1) 结构化绑定（pair / tuple / 结构体 / 数组 / map）");
  {
    const std::pair<int, std::string> p{1, "idle"};
    const auto& [code, name] = p;
    MINI_INFO("pair    : code={}, name={}", code, name);

    const std::tuple<int, std::string, bool> t{3, "download", true};
    const auto& [idx, state_name, flag] = t;
    MINI_INFO("tuple   : idx={}, state={}, flag={}", idx, state_name, flag);

    const MotorType motor{1, 2, 0, "ethercat-canfd"};
    const auto& [master, slave, canfd, proto] = motor;
    MINI_INFO("struct  : master={}, slave={}, canfd={}, proto={}", master, slave, canfd, proto);

    const int arr[3] = {10, 20, 30};
    const auto& [a0, a1, a2] = arr;
    MINI_INFO("array   : {} {} {}", a0, a1, a2);

    const std::map<std::string, int> versions{{"mcu", 103}, {"rk3588", 210}};
    for (const auto& [k, v] : versions) {   // ← 工程里 for (const auto& [c, desc] : table) 同款
      MINI_INFO("map     : {} -> {}", k, v);
    }

    // 结构化绑定 + if 初始化：判断某个 key 是否存在
    if (const auto it = versions.find("mcu"); it != versions.end()) {
      const auto& [k, v] = *it;
      MINI_INFO("find    : {} = {}", k, v);
    }
  }

  printStep("2) if 初始化语句：把变量限制在 if 作用域内");
  {
    auto readStateFromFile = [](const std::string& path) -> std::optional<int> {
      if (path == "/no/such/file") return std::nullopt;
      return 3;
    };
    // 对应 parsing_img.cpp:
    //   if (auto artifact = getTaskArtifact(task_type); artifact.has_value()) { ... }
    if (const auto state = readStateFromFile("/home/eame/.../current_ota_state"); state.has_value()) {
      MINI_INFO("读到的状态 = {}", state.value());
    } else {
      MINI_WARN("没读到状态文件，走 INIT");
    }
    // MINI_INFO("{}", state.value());  // ❌ state 已出作用域
  }

  printStep("3) if constexpr：编译期分支（未命中的分支不实例化）");
  {
    MINI_INFO("{}", describe(42));
    MINI_INFO("{}", describe(3.14));
    MINI_INFO("{}", describe("hello"));
    MINI_INFO("add(1, 2.5) = {} (尾置返回类型推导出 double)", add(1, 2.5));
  }

  printStep("4) [[likely]] / [[unlikely]]：给分支预测器和编译器提示");
  {
    auto processResult = [](bool ok) {
      // 真实工程：if (!is_success) [[unlikely]] { std::terminate(); }
      if (!ok) [[unlikely]] {
        return std::string("失败（罕见路径）");
      }
      return std::string("成功（常见路径）");
    };
    MINI_INFO("ok=true  -> {}", processResult(true));
    MINI_INFO("ok=false -> {}", processResult(false));
  }

  printStep("5) [[nodiscard]]：忘记检查返回值会告警");
  {
    // 编译时会有 -Wunused-result 告警（本行故意演示，正常代码不应忽略）
    (void)checkBattery(10);
    const auto ret = checkBattery(10);
    MINI_INFO("电量 10% -> {}", ret == ErrorCode::OK ? "OK" : "FAILED(电量不足)");
  }

  printStep("6) 初始化捕获 + 移动捕获");
  {
    int base = 100;
    const auto doubled = [n = base * 2] { return n; };          // C++14
    auto ptr = std::make_unique<int>(7);
    // 把 unique_ptr 移进 lambda（C++14 起支持）
    const auto owner = [p = std::move(ptr)] { return *p; };
    MINI_INFO("初始化捕获 n=base*2 = {}", doubled());
    MINI_INFO("移动捕获 *p          = {}", owner());
  }

  printStep("7) std::variant + std::visit（类型安全的 union）");
  {
    const std::vector<UpgradePayload> payloads = {
        std::monostate{},
        std::string("mcu_v1.0.3.bin"),
        std::pair<std::string, std::string>{"2.1.0", "https://ota.example.com/media/img.zip"},
    };
    for (const auto& p : payloads) {
      MINI_INFO("{}", describePayload(p));
    }

    // 判断当前是哪种类型
    const UpgradePayload v = std::string("rk_motor.bin");
    MINI_INFO("is monostate? {}, holds<string>? {}, holds<pair>? {}",
              std::holds_alternative<std::monostate>(v),
              std::holds_alternative<std::string>(v),
              std::holds_alternative<std::pair<std::string, std::string>>(v));
    // 已知类型时直接取（类型不对会抛 bad_variant_access）
    MINI_INFO("get<string> = {}", std::get<std::string>(v));
  }

  printStep("8) 算法库小工具：clamp / transform / any_of / accumulate");
  {
    const int raw_battery = 130;
    const int clamped = std::clamp(raw_battery, 0, 100);   // 裁剪到 [0,100]
    MINI_INFO("clamp(130, 0, 100) = {}", clamped);

    std::vector<std::string> task_names{"nx_serial", "rk_ethercat", "rk_self", "nx_self"};
    std::transform(task_names.begin(), task_names.end(), task_names.begin(),
                   [](std::string s) {
                     for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                     return s;
                   });
    std::cout << "    转换后任务列表: ";
    for (const auto& n : task_names) std::cout << n << " ";
    std::cout << "\n";

    const bool has_rk = std::any_of(task_names.begin(), task_names.end(),
                                    [](const std::string& s) { return s.rfind("RK", 0) == 0; });
    MINI_INFO("是否存在 RK 类任务: {}", has_rk);
  }

  printStep("9) [[fallthrough]]：故意不写 break 时必须标注");
  {
    auto classify = [](int level) -> std::string {
      std::string result;
      switch (level) {
        case 3:
          result += "L3 ";
          [[fallthrough]];   // ← 显式声明"我要穿透"，否则编译器/静态检查会警告
        case 2:
          result += "L2 ";
          [[fallthrough]];
        case 1:
          result += "L1";
          break;
        default:
          result = "unknown";
          break;
      }
      return result;
    };
    MINI_INFO("classify(3) = {} (穿透生效)", classify(3));
    MINI_INFO("classify(1) = {}", classify(1));
  }

  printStep("10) string_view 字面量 与 字符串前缀判断");
  {
    constexpr std::string_view kStateFile = "/home/eame/cust_para/ota_file/ota_work/current_ota_state"sv;
    MINI_INFO("string_view 字面量: {}", kStateFile.substr(kStateFile.find_last_of('/') + 1));
    MINI_INFO("starts_with(\"/home\") = {}", kStateFile.starts_with("/home"));   // C++20
    MINI_INFO("ends_with(\"state\")  = {}", kStateFile.ends_with("state"));      // C++20
  }

  printStep("小结");
  std::cout << R"(
    - 结构化绑定：const auto& [a, b] = x;  省掉 .first/.second，map 遍历必备
    - if 初始化语句：if (init; cond) 变量作用域被限制在 if/else 内，减少污染
    - if constexpr：编译期分支，未命中的分支不会实例化（模板里替代 SFINAE 的利器）
    - [[likely]]/[[unlikely]]：分支预测提示，热路径上能提升几个百分点
    - [[nodiscard]]：强制检查返回值，对 ErrorCode/expected 这类返回值非常有用
    - 初始化捕获 [n = expr] / 移动捕获 [p = std::move(q)]：C++14 起
    - std::variant + std::visit + Overloaded 技巧：类型安全的 union，替代裸 union
    - std::clamp / std::any_of / std::transform：能用标准算法就别手写 for
    - [[fallthrough]]：switch 故意穿透要标注，否则静态检查报警
    - C++20 的 starts_with/ends_with + "xxx"sv：字符串处理更干净
)";
  return 0;
}
