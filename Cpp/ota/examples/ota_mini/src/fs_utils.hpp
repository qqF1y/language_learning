/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/fs_utils.hpp
 * @Brief: 文件/目录工具 —— 对应真实工程 src/utils/filesystem
 *
 * 语法点：
 *   std::filesystem（含 error_code 版本）、POSIX 原子写、可后缀过滤的目录扫描
 ***************************************************************/
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "result.hpp"

namespace ota_mini::fs {

// 递归创建目录（幂等）
ExpectedVoid<ErrorCode> ensureDir(const std::filesystem::path& dir);

// 原子写：写 .tmp(O_SYNC) -> 老文件改名 .last -> tmp 改名成正式名
ExpectedVoid<ErrorCode> atomicWriteFile(const std::filesystem::path& path, const std::string& data);

// 读整个文件
Expected<std::string, ErrorCode> readFile(const std::filesystem::path& path);

// 递归找指定后缀的文件（对应 parsing_img.cpp 的 hasUpgradeArtifacts）
std::vector<std::filesystem::path> findFilesWithExtension(const std::filesystem::path& dir,
                                                          const std::string& extension);

// 目录是否存在且非空（对应"升级包里到底有没有这个模块的固件"）
bool dirHasAnyFile(const std::filesystem::path& dir);

// 递归删除目录（幂等）
void removeDirIfExists(const std::filesystem::path& dir);

}  // namespace ota_mini::fs
