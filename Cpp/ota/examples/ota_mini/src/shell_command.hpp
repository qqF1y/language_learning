/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/shell_command.hpp
 * @Brief: 执行外部脚本 —— 对应真实工程 src/utils/shell_command
 *
 * 真实工程用它来跑 scripts/nx、scripts/rk3588 下的升级脚本。
 * 本例程在运行时自己生成假脚本，用来演示 popen / SIGPIPE / 退出码解析。
 ***************************************************************/
#pragma once

#include <string>
#include <string_view>

#include "result.hpp"

namespace ota_mini::shell_command {

// 执行命令并实时收集输出。
//   command: 可执行文件或脚本路径（含空格的命令会原样当 shell 命令行处理）
//   params : 追加参数
//   output : 非空则把全部输出回填给它
ErrorCode execute(std::string_view command, std::string_view params = {}, std::string* output = nullptr);

// 生成一个"假升级脚本"，内容根据参数不同而不同（用于演示成功/失败两种情况）
void createFakeScript(const std::string& path, int exit_code, const std::string& message);

}  // namespace ota_mini::shell_command
