/*************************************************************
 * @FilePath: /Cpp/ota/syntax/09_string_view_format.cpp
 * @Brief: 语法点 09 —— std::string_view、字符串查找切分、变参模板模拟 fmt
 *
 * 真实工程出处：
 *   - src/utils/filesystem/filesystem_wrapper.hpp
 *       bool isFileExists(std::string_view path);           // ← 全用 string_view 收参
 *       ErrorCode deleteFile(std::string_view path);
 *   - src/utils/filesystem/file_operator.hpp
 *       static ErrorCode updateFile(std::string_view path, std::string_view data);
 *   - src/ota/task/task.cpp
 *       std::stringstream ss(content);  while (ss >> task_type_str) { ... }   // 空格分隔切词
 *       std::transform(s.begin(), s.end(), s.begin(), ::tolower);
 *   - 全工程 fmt::format("{} {} ", a, b)
 *
 * std::string_view 是什么？
 *   (指针, 长度) 的只读视图，不拥有内存、不拷贝。
 *   传参用它替代 const std::string&，可避免"字符串字面量 -> std::string"的临时对象分配。
 *   ⚠️ 生命周期：它不延长被引用字符串的寿命，不能返回指向局部变量的 string_view。
 ***************************************************************/

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "mini_log.hpp"

namespace ota::utils {

// ===========================================================================
// 1) string_view 参数：零拷贝
// ===========================================================================
void printPath(std::string_view path) {
  MINI_INFO("path = {} (长度 {}, 是否为空 {})", path, path.size(), path.empty());
}

// 危险示范：返回指向局部变量的 string_view -> 悬空！
// std::string_view badReturn() { std::string local = "abc"; return local; }  // ❌

// 安全：两个 string_view 拼接必须产生新的 std::string
std::string joinPath(std::string_view dir, std::string_view name) {
  std::string out{dir};
  out.append(name);     // 或 out += name;
  return out;
}

// ===========================================================================
// 2) string_view 的查找 / 切分（对应 Download 状态里取 URL 文件名）
// ===========================================================================
std::string_view fileNameFromUrl(std::string_view url) {
  const auto pos = url.find_last_of('/');
  if (pos == std::string_view::npos) {
    return url;
  }
  return url.substr(pos + 1);   // substr 也是零拷贝（返回视图）
}

// 对应 download.cpp：
//   std::string download_file_name =
//       getPackageUrl().substr(getPackageUrl().find_last_of('/') + 1);
std::string fileNameFromUrlCopy(const std::string& url) {
  return url.substr(url.find_last_of('/') + 1);
}

// ===========================================================================
// 3) 按空格切词（对应 task.cpp 用 stringstream 解析任务列表文件）
//    这里给出 string_view 版本，完全不分配内存
// ===========================================================================
std::vector<std::string_view> splitBySpace(std::string_view text) {
  std::vector<std::string_view> tokens;
  std::size_t i = 0;
  while (i < text.size()) {
    // 跳过空白
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    const std::size_t start = i;
    while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    if (i > start) {
      tokens.push_back(text.substr(start, i - start));
    }
  }
  return tokens;
}

// ===========================================================================
// 4) 字符串前缀/后缀判断（对应 shell_command.cpp 判断 .sh / .py）
// ===========================================================================
bool endsWith(std::string_view s, std::string_view suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// ===========================================================================
// 5) 手写一个 fmt::format 的迷你实现，看清"变参模板 + 折叠表达式"怎么工作
//    真实工程用的是 fmt/format.h，接口是 fmt::format("{} {}", a, b)
// ===========================================================================
void replaceOnePlaceholder(std::string& out, std::size_t& cursor, const std::string& value) {
  const auto pos = out.find("{}", cursor);
  if (pos == std::string::npos) {
    return;
  }
  out.replace(pos, 2, value);
  cursor = pos + value.size();
}

template <typename... Args>
std::string myFormat(std::string_view pattern, const Args&... args) {
  std::string out{pattern};
  std::size_t cursor = 0;
  // 折叠表达式：把每个 arg 对应的调用按逗号依次展开
  //   ((void)replaceOne(...), ...)
  (replaceOnePlaceholder(out, cursor, mini::toStr(args)), ...);
  return out;
}

}  // namespace ota::utils

int main() {
  using namespace ota::utils;
  printTitle("09 string_view / 字符串处理 / 变参模板格式化");
  printSourceHint("src/utils/filesystem/*.hpp, src/ota/task/task.cpp, src/ota/state_machine/download.cpp");

  printStep("1) string_view 零拷贝传参");
  // 字面量直接传，不构造 std::string（const std::string& 版本会构造临时对象）
  printPath("/home/eame/cust_para/ota_file/img");
  printPath({});
  std::string owned = "/opt/eame/log/humanoid_ota/ota.log";
  printPath(owned);  // std::string 隐式转 string_view，同样不拷贝

  printStep("2) substr 不分配内存");
  const std::string_view url = "https://ota.example.com/media/img.zip";
  const auto name_view = fileNameFromUrl(url);
  const auto name_copy = fileNameFromUrlCopy(std::string(url));
  MINI_INFO("URL           : {}", url);
  MINI_INFO("string_view 结果: {}  (size={}, 无分配)", name_view, name_view.size());
  MINI_INFO("string 结果     : {}  (size={}, 有一次 substr 分配)", name_copy, name_copy.size());

  printStep("3) 按空格切词（对应任务列表文件 \"nx_serial rk_ethercat rk_self\"）");
  const std::string_view task_file = "nx_serial rk_ethercat rk_self  nx_self ";
  for (const auto token : splitBySpace(task_file)) {
    MINI_INFO("token = [{}]", token);
  }

  printStep("4) 后缀判断（对应 shell_command 里判断 .sh/.py）");
  for (const std::string_view cmd : {"upgrade.sh", "query_version.py", "ota_node"}) {
    MINI_INFO("cmd={}  .sh?={}  .py?={}", cmd, endsWith(cmd, ".sh"), endsWith(cmd, ".py"));
  }

  printStep("5) 迷你 fmt::format：变参模板 + 折叠表达式");
  const int progress = 42;
  const std::string device = "/dev/ttyTHS1";
  const bool ok = true;
  MINI_INFO("myFormat: {}", myFormat("设备 {} 进度 {}% 成功 {}", device, progress, ok));
  MINI_INFO("少给参数也不会崩: [{}]", myFormat("A={} B={}", 1));
  MINI_INFO("多给参数会被忽略 : [{}]", myFormat("A={}", 1, 2, 3));

  printStep("6) string_view vs string 的性能对比（数量级感受）");
  constexpr int kN = 200000;
  {
    // 用 const std::string& 收参：字面量 -> 临时 std::string（堆分配）
    const auto t0 = std::chrono::steady_clock::now();
    std::size_t total = 0;
    for (int i = 0; i < kN; ++i) {
      total += std::string("/home/eame/cust_para/ota_file/img").size();
    }
    const auto cost1 = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::steady_clock::now() - t0).count();
    (void)total;

    // 用 string_view：零分配
    const auto t1 = std::chrono::steady_clock::now();
    std::size_t total2 = 0;
    std::string_view sv = "/home/eame/cust_para/ota_file/img";
    for (int i = 0; i < kN; ++i) {
      total2 += sv.size();
    }
    const auto cost2 = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::steady_clock::now() - t1).count();
    (void)total2;
    MINI_INFO("构造 std::string ×{}: {} us", kN, cost1);
    MINI_INFO("string_view 取 size ×{}: {} us", kN, cost2);
  }

  printStep("小结");
  std::cout << R"(
    - std::string_view = (ptr, len) 视图，不拥有内存 -> 绝不返回指向局部变量的 view
    - 只读字符串参数统一用 string_view 收（比 const string& 更通用、更省）
    - 需要"拥有"或"修改"时才用 std::string
    - substr / find / compare 都不会重新分配
    - 变参模板 + 折叠表达式是 fmt 这类格式化库的基石：
        (expr(args), ...)      逗号折叠，从左到右求值
        (... + args)           加折叠，求和
        (f(args) && ...)       逻辑折叠，全部为真
    - 注意 #include <cctype> 后 std::tolower 要传 unsigned char，否则是 UB
)";
  return 0;
}
