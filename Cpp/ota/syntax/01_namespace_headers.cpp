/*************************************************************
 * @FilePath: /Cpp/ota/syntax/01_namespace_headers.cpp
 * @Brief: 语法点 01 —— 命名空间 / 头文件组织 / 链接性
 *
 * 真实工程出处：
 *   - src/utils/ros2/ros2.hpp   : 一整个 namespace humanoid_ota::utils::ros2
 *   - src/utils/ros2/ros2.cpp   : 函数内 static 常量 + 返回 const std::string&
 *   - src/ota/state_machine/parsing_img.cpp : 匿名 namespace 放内部工具函数
 *
 * 编译运行：
 *   g++ -std=c++20 -O2 01_namespace_headers.cpp -o 01 && ./01
 ***************************************************************/

#include <iostream>
#include <string>
#include <string_view>

#include "mini_log.hpp"

// ===========================================================================
// 1) 嵌套命名空间：C++17 起可以一次写完，不用层层嵌套大括号
//    等价于 namespace humanoid_ota { namespace utils { namespace ros2 { ... } } }
// ===========================================================================
namespace humanoid_ota::utils::ros2 {

// ---------------------------------------------------------------------------
// 2) 函数内 static + 返回 const 引用
//    作用：常量只构造一次；调用方拿到的是引用，零拷贝。
//    线程安全：C++11 起，函数内静态变量的初始化由编译器加锁保证只执行一次
//    （俗称 "magic static"）。
//    注意：不要返回"值"，否则每次调用都拷贝一个字符串。
// ---------------------------------------------------------------------------
const std::string& getOtaWorkDir() {
  static const std::string dir = "/home/eame/cust_para/ota_file/ota_work";
  return dir;
}

const std::string& getCurrentOtaStateFile() {
  // 依赖上一个函数的返回值：因为也是 static，只会拼接一次
  static const std::string file = getOtaWorkDir() + "/current_ota_state";
  return file;
}

// ---------------------------------------------------------------------------
// 3) 命名空间可以"再打开"（reopen），分散到多个 .hpp/.cpp 里
//    工程里 ros2.hpp 声明、ros2.cpp 定义，就是靠这个特性。
// ---------------------------------------------------------------------------
std::string describe();  // 先声明

}  // namespace humanoid_ota::utils::ros2

// 重新打开同一命名空间来定义（真实工程里这发生在另一个 .cpp 文件）
namespace humanoid_ota::utils::ros2 {
std::string describe() {
  return "ros2 utils namespace reopened";
}
}  // namespace humanoid_ota::utils::ros2

// ===========================================================================
// 4) 匿名命名空间 = 内部链接（internal linkage）
//    只在本翻译单元可见，可替代 static 函数 / static 全局变量。
//    真实工程 parsing_img.cpp 里用它放 getTaskArtifact / hasUpgradeArtifacts。
// ===========================================================================
namespace {

int g_internalCounter = 0;  // 外部文件看不到，不会符号冲突

std::string helperMakeName(std::string_view base) {
  return std::string(base) + "_" + std::to_string(++g_internalCounter);
}

}  // namespace

// ===========================================================================
// 5) namespace 别名：给长命名空间起短名，仅影响当前文件
// ===========================================================================
namespace ros2 = humanoid_ota::utils::ros2;

int main() {
  printTitle("01 命名空间 / 头文件 / 链接性");
  printSourceHint("src/utils/ros2/ros2.hpp|.cpp, src/ota/state_machine/parsing_img.cpp");

  // ---- 限定名访问（qualified lookup）----
  printStep("1) 嵌套命名空间 + const& 返回静态对象");
  // 用 auto& 接住引用，避免拷贝
  const std::string& work_dir = humanoid_ota::utils::ros2::getOtaWorkDir();
  MINI_INFO("工作目录       : {}", work_dir);
  MINI_INFO("状态文件路径   : {}", humanoid_ota::utils::ros2::getCurrentOtaStateFile());
  MINI_INFO("两次调用是同一对象吗? {}", static_cast<const void*>(&work_dir) ==
                                        static_cast<const void*>(&humanoid_ota::utils::ros2::getOtaWorkDir()));

  // ---- namespace 别名 ----
  printStep("2) namespace 别名");
  MINI_INFO("通过别名调用   : {}", ros2::describe());

  // ---- 匿名命名空间 ----
  printStep("3) 匿名命名空间（内部链接）");
  MINI_INFO("内部函数生成   : {}", helperMakeName("mcu"));
  MINI_INFO("内部函数生成   : {}", helperMakeName("mcu"));
  MINI_INFO("内部计数器     : {}", g_internalCounter);

  // ---- using 声明 vs using namespace ----
  printStep("4) using 声明（推荐） vs using namespace（不推荐）");
  {
    using humanoid_ota::utils::ros2::getOtaWorkDir;  // 只引入单个名字，作用域限于本块
    MINI_INFO("using 声明后直接调用: {}", getOtaWorkDir());
  }
  // MINI_INFO("{}", getOtaWorkDir());  // 编译错误：出了作用域就不可见

  printStep("小结");
  std::cout << R"(
    - #pragma once  : 头文件只包含一次（比 include guard 简洁，非标准但各家编译器都支持）
    - 嵌套命名空间 : namespace a::b::c { }   （C++17）
    - 返回 const&  : 配合函数内 static，常量只构造一次且零拷贝
    - 匿名 namespace: 内部链接，替代 static
    - namespace 别名: 只能在当前文件内使用
    - using 声明   : 精细引入单个名字；using namespace 会污染作用域
)";

  return 0;
}
