/*************************************************************
 * @FilePath: /Cpp/ota/syntax/14_popen_signal_pipe.cpp
 * @Brief: 语法点 14 —— popen/pclose 执行外部命令、读输出、SIGPIPE 处理
 *
 * 真实工程出处（src/utils/shell_command/shell_command.cpp 逐行对照）：
 *
 *   ErrorCode execute(std::string_view command, std::string_view params, std::string* output) {
 *     int status = 0;
 *
 *     // ① 忽略 SIGPIPE：子进程提前退出时，父进程写管道会收到 SIGPIPE，
 *     //    默认动作是"杀死进程"。这里显式忽略，改为让 write 返回 EPIPE。
 *     struct sigaction sa;
 *     sa.sa_handler = SIG_IGN;
 *     sigemptyset(&sa.sa_mask);
 *     sa.sa_flags = 0;
 *     sigaction(SIGPIPE, &sa, nullptr);
 *
 *     auto pipe_close = [&](FILE* pipe) { if (pipe) status = pclose(pipe); };
 *     std::unique_ptr<FILE, decltype(pipe_close)> pipe(nullptr, pipe_close);  // ② RAII 保证 pclose
 *     std::array<char, 2048> buffer;
 *
 *     // ③ 根据后缀决定怎么执行：.py -> python3, .sh -> bash
 *     if (command.size() > 3 && command.substr(command.size() - 3) == ".py")
 *       full_command = fmt::format("python3 {} {}", command, params);
 *     else if (... ".sh") full_command = fmt::format("bash {} {}", command, params);
 *
 *     // ④ 把子进程的返回值也回显到 stdout，这样日志里能看到脚本退出码
 *     full_command = fmt::format("{} 2>&1; return_value=$?; echo \"\n脚本返回值: $return_value\"; exit $return_value",
 *                                full_command);
 *
 *     pipe.reset(popen(full_command.c_str(), "r"));   // ⑤ "r" = 读子进程 stdout
 *
 *     while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {   // ⑥ 逐行读
 *       std::string line(buffer.data());
 *       ss << line;                                    // 汇总到 stringstream 供日志
 *       if (output) output->append(buffer.data());     // 输出可选返回给调用方
 *     }
 *
 *     // ⑦ 解析退出码：status 在 pclose 后才有值，0 表示成功
 *     if (status == 0) return ErrorCode::OK;
 *     ... 从最后一行里扒出错码，映射成自定义 ErrorCode ...
 *   }
 *
 * 关键点：
 *   system()      : 只关心成功/失败，拿不到输出
 *   popen/pclose  : 能读子进程 stdout（"r"）或写 stdin（"w"），返回值和 shell 一样是 wait 状态
 *   SIGPIPE       : 不忽略的话，子进程退出后父进程直接被杀
 ***************************************************************/

#include <signal.h>
#include <sys/wait.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "mini_log.hpp"

namespace ota::utils::shell_command {

enum class ErrorCode { OK, FAILED, TIMEOUT, NOT_FOUND };

// ===========================================================================
// 忽略 SIGPIPE：任何 shell 封装函数都应该做的第一件事
// ===========================================================================
void ignoreSigpipe() {
  struct sigaction sa {};
  sa.sa_handler = SIG_IGN;      // 设为忽略（而不是默认的终止进程）
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGPIPE, &sa, nullptr) != 0) {
    MINI_ERROR("sigaction(SIGPIPE) 失败: {}", std::strerror(errno));
  }
}

// ===========================================================================
// execute：和真实工程同构的实现
// ===========================================================================
ErrorCode execute(std::string_view command, std::string_view params, std::string* output = nullptr) {
  int status = 0;
  std::stringstream ss;          // 汇总全部输出，最后打日志
  std::string last_line;         // 用于解析脚本自定义退出码

  ignoreSigpipe();

  // ---- ① 按后缀决定解释器 ----
  std::string full_command;
  if (command.find(' ') != std::string_view::npos) {
    // 命令里已经带参数了，原样拼
    full_command = std::string(command) + " " + std::string(params);
  } else if (command.size() > 3 && command.substr(command.size() - 3) == ".py") {
    full_command = "python3 " + std::string(command) + " " + std::string(params);
  } else if (command.size() > 3 && command.substr(command.size() - 3) == ".sh") {
    full_command = "bash " + std::string(command) + " " + std::string(params);
  } else {
    full_command = std::string(command) + " " + std::string(params);
  }

  // ---- ② 让子进程把退出码也打印出来 ----
  // 2>&1: 把 stderr 合并到 stdout，日志里不会丢错误信息
  full_command = full_command +
                 " 2>&1; return_value=$?;"
                 " echo \"\n脚本返回值: $return_value\"; exit $return_value";

  MINI_INFO("full_command: {}", full_command);

  // ---- ③ RAII 包住 FILE*，保证异常路径也会 pclose ----
  auto pipe_close = [&status](FILE* p) {
    if (p != nullptr) {
      status = pclose(p);                 // ← pclose 的返回值才是子进程状态
      if (status == -1) {
        MINI_ERROR("pclose 失败: {}", std::strerror(errno));
      }
    }
  };
  std::unique_ptr<FILE, decltype(pipe_close)> pipe(nullptr, pipe_close);
  std::array<char, 2048> buffer{};

  pipe.reset(popen(full_command.c_str(), "r"));   // "r" = 读子进程 stdout
  if (!pipe) {
    MINI_ERROR("popen 失败: {}", std::strerror(errno));
    return ErrorCode::NOT_FOUND;
  }

  // ---- ④ 逐行读，直到子进程关闭管道 ----
  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
    std::string line(buffer.data());
    ss << line;
    if (!line.empty() && line.back() == '\n') {
      line.pop_back();
    }
    // 真实工程的小技巧：短行拼接，长行覆盖，用来定位"最后一条有意义的输出"
    if (line.size() < 10) {
      last_line += line;
    } else {
      last_line = line;
    }
    if (output != nullptr) {
      output->append(buffer.data());
    }
  }

  MINI_INFO("脚本原始输出:\n{}", ss.str());

  // ---- ⑤ 解析状态：status 由 pclose 填好 ----
  if (status == -1) {
    return ErrorCode::FAILED;
  }
  if (status == 0) {
    return ErrorCode::OK;
  }

  // WIFEXITED / WEXITSTATUS 是标准做法（比直接 status >> 8 可读）
  if (WIFEXITED(status)) {
    const int exit_code = WEXITSTATUS(status);
    MINI_WARN("子进程正常退出，退出码 = {}", exit_code);
    return exit_code == 0 ? ErrorCode::OK : ErrorCode::FAILED;
  }
  if (WIFSIGNALED(status)) {
    MINI_ERROR("子进程被信号 {} 杀死", WTERMSIG(status));
    return ErrorCode::FAILED;
  }

  // ---- ⑥ 真实工程的"约定"：脚本自定义错误码 ----
  // 从最后一行抠出 ": <数字>" 后面的数字，映射成自己的 ErrorCode
  MINI_INFO("last_line = [{}]", last_line);
  const auto pos = last_line.find_last_of(": ");
  const std::string code_str = (pos != std::string::npos) ? last_line.substr(pos + 1) : "255";
  MINI_INFO("解析出的脚本错误码 = {}", code_str);
  return ErrorCode::FAILED;
}

const char* toString(ErrorCode code) {
  switch (code) {
    case ErrorCode::OK: return "OK";
    case ErrorCode::FAILED: return "FAILED";
    case ErrorCode::TIMEOUT: return "TIMEOUT";
    case ErrorCode::NOT_FOUND: return "NOT_FOUND";
  }
  return "?";
}

}  // namespace ota::utils::shell_command

int main() {
  using namespace ota::utils::shell_command;
  printTitle("14 popen/pclose / 读子进程输出 / SIGPIPE");
  printSourceHint("src/utils/shell_command/shell_command.cpp");

  printStep("1) 执行成功：echo，输出被逐行读回来");
  {
    std::string out;
    const auto ret = execute("echo", "hello humanoid ota", &out);
    MINI_INFO("返回 = {}, 捕获输出 = [{}]", toString(ret), out.substr(0, out.find('\n')));
  }

  printStep("2) 读取真实命令输出：uname -a（对应下载完执行 unzip）");
  {
    std::string out;
    const auto ret = execute("uname", "-a", &out);
    MINI_INFO("返回 = {}", toString(ret));
    std::istringstream iss(out);
    std::string first;
    std::getline(iss, first);
    MINI_INFO("第一行: {}", first);
  }

  printStep("3) 命令失败：退出码非 0");
  {
    std::string out;
    // sh -c 'exit 3' -> 子进程退出码 3
    const auto ret = execute("sh", "-c 'exit 3'", &out);
    MINI_INFO("返回 = {} (FAILED 符合预期)", toString(ret));
  }

  printStep("4) 命令不存在：popen 能成功（是 shell 报的错），但退出码非 0");
  {
    std::string out;
    const auto ret = execute("no_such_command_xyz", "", &out);
    MINI_INFO("返回 = {}, shell 的报错被 2>&1 捕获: {}",
              toString(ret), out.substr(0, out.find('\n')));
  }

  printStep("5) 不捕获输出（output = nullptr）也能正常跑");
  {
    const auto ret = execute("true", "");
    MINI_INFO("true  -> {}", toString(ret));
    const auto ret2 = execute("false", "");
    MINI_INFO("false -> {}", toString(ret2));
  }

  printStep("6) SIGPIPE 演示：子进程提前退出，父进程仍存活");
  {
    // head -1 读完第一行就退出并关闭管道；后续父进程再写就会触发 SIGPIPE。
    // 我们已经在 execute() 里忽略，所以这里不会整个进程被杀。
    std::string out;
    const auto ret = execute("sh", "-c 'echo line1; echo line2; exit 0'", &out);
    MINI_INFO("返回 = {}，父进程依然活着（没被 SIGPIPE 杀掉）", toString(ret));
    MINI_INFO("收到输出: {}", out.substr(0, out.find('\n')));
  }

  printStep("7) 写一个临时脚本 -> 按 .sh 规则走 bash 执行");
  {
    const std::string script = "/tmp/ota_shell_demo.sh";
    {
      FILE* f = std::fopen(script.c_str(), "w");
      std::fputs("#!/bin/bash\necho \"脚本收到参数: $1\"\nexit 0\n", f);
      std::fclose(f);
    }
    std::string out;
    const auto ret = execute(script, "RK3588", &out);
    MINI_INFO("返回 = {}", toString(ret));
    MINI_INFO("输出 = {}", out.substr(0, out.find('\n')));
    std::remove(script.c_str());
  }

  printStep("小结");
  std::cout << R"(
    - popen(cmd, "r") 读子进程 stdout；"w" 则写它的 stdin
    - 必须配 pclose，否则产生僵尸进程（zombie），工程里用 unique_ptr + 自定义析构保证
    - pclose 的返回值才是子进程状态，要用 WIFEXITED/WEXITSTATUS 解析（不是 pclose 的直接值）
    - 第一步就 sigaction(SIGPIPE, SIG_IGN)：否则子进程先退出会把父进程杀掉
    - fgets 逐行读：管道是流，读到的行可能被截断（buffer 2048 不够时）
    - 常见坑：command 里含空格时不要加引号，交给 shell 处理即可
    - 常见坑：popen 成功 ≠ 命令成功，必须看退出码
)";
  return 0;
}
