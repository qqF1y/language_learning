/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/shell_command.cpp
 * @Brief: popen/pclose 执行脚本
 *
 * 与真实工程 src/utils/shell_command/shell_command.cpp 结构一致：
 *   ① 忽略 SIGPIPE（否则子进程提前退出会杀掉父进程）
 *   ② unique_ptr<FILE, lambda> 保证 pclose 一定被调用（不产生僵尸进程）
 *   ③ 按后缀决定 python3 / bash
 *   ④ 把退出码同时回显到 stdout，日志里能看到
 *   ⑤ fgets 逐行读
 *   ⑥ 用 WIFEXITED/WEXITSTATUS 解析状态
 ***************************************************************/

#include "shell_command.hpp"

#include <signal.h>
#include <sys/wait.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>

#include "log.hpp"

namespace ota_mini::shell_command {
namespace {

void ignoreSigpipe() {
  struct sigaction sa {};
  sa.sa_handler = SIG_IGN;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGPIPE, &sa, nullptr);
}

}  // namespace

ErrorCode execute(std::string_view command, std::string_view params, std::string* output) {
  int status = 0;
  std::stringstream collected;

  ignoreSigpipe();

  // ---- ① 组装完整命令 ----
  std::string full_command;
  if (command.find(' ') != std::string_view::npos) {
    full_command = std::string(command);
  } else if (command.size() > 3 && command.substr(command.size() - 3) == ".py") {
    full_command = "python3 " + std::string(command);
  } else if (command.size() > 3 && command.substr(command.size() - 3) == ".sh") {
    full_command = "bash " + std::string(command);
  } else {
    full_command = std::string(command);
  }
  if (!params.empty()) {
    full_command += " " + std::string(params);
  }

  // ---- ② 让子进程把退出码打印出来（2>&1 把 stderr 并到 stdout）----
  full_command += " 2>&1; return_value=$?; echo \"脚本返回值: $return_value\"; exit $return_value";

  OTA_LOG_DEBUG("full_command: {}", full_command);

  // ---- ③ RAII 包住 FILE* ----
  auto pipeReleaser = [&status](FILE* p) {
    if (p != nullptr) {
      status = pclose(p);              // ← 只有 pclose 的返回值才是子进程状态
    }
  };
  std::unique_ptr<FILE, decltype(pipeReleaser)> pipe(nullptr, pipeReleaser);
  std::array<char, 1024> buffer{};

  pipe.reset(popen(full_command.c_str(), "r"));
  if (!pipe) {
    OTA_LOG_ERROR("popen 失败: {} ({})", full_command, std::strerror(errno));
    return ErrorCode::FAILED;
  }

  // ---- ④ 逐行读 ----
  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
    collected << buffer.data();
    if (output != nullptr) {
      output->append(buffer.data());
    }
  }

  if (status == -1) {
    OTA_LOG_ERROR("pclose 失败: {}", std::strerror(errno));
    return ErrorCode::FAILED;
  }
  if (status == 0) {
    return ErrorCode::OK;
  }

  // ---- ⑤ 解析退出状态 ----
  if (WIFEXITED(status)) {
    const int exit_code = WEXITSTATUS(status);
    OTA_LOG_WARN("脚本 {} 退出码 = {}\n{}", command, exit_code, collected.str());
    return ErrorCode::FAILED;
  }
  if (WIFSIGNALED(status)) {
    OTA_LOG_ERROR("脚本被信号 {} 终止", WTERMSIG(status));
    return ErrorCode::FAILED;
  }
  return ErrorCode::FAILED;
}

void createFakeScript(const std::string& path, int exit_code, const std::string& message) {
  std::ofstream ofs(path);
  ofs << "#!/bin/bash\n"
      << "echo \"[fake-script] " << message << "\"\n"
      << "sleep 0.05\n"
      << "exit " << exit_code << "\n";
}

}  // namespace ota_mini::shell_command
