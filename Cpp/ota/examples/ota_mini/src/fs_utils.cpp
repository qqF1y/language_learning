/*************************************************************
 * @FilePath: /Cpp/ota/examples/ota_mini/src/fs_utils.cpp
 * @Brief: 文件/目录工具实现
 ***************************************************************/

#include "fs_utils.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <system_error>

#include "log.hpp"

namespace ota_mini::fs {
namespace {

// POSIX 裸写：支持 O_SYNC，循环写直到写完
ErrorCode writeRaw(const std::filesystem::path& path, const std::string& data, bool sync) {
  int flags = O_WRONLY | O_CREAT | O_TRUNC;
  if (sync) {
    flags |= O_SYNC;
  }
  const int fd = ::open(path.c_str(), flags, 0666);
  if (fd == -1) {
    OTA_LOG_ERROR("open 失败: {} ({})", path.string(), std::strerror(errno));
    return ErrorCode::WRITE_FAILED;
  }

  std::size_t written_total = 0;
  while (written_total < data.size()) {
    const ssize_t n = ::write(fd, data.data() + written_total, data.size() - written_total);
    if (n == -1) {
      OTA_LOG_ERROR("write 失败: {} ({})", path.string(), std::strerror(errno));
      ::close(fd);
      return ErrorCode::WRITE_FAILED;
    }
    written_total += static_cast<std::size_t>(n);
  }

  if (!sync) {
    ::fsync(fd);
  }
  ::close(fd);
  return ErrorCode::OK;
}

}  // namespace

ExpectedVoid<ErrorCode> ensureDir(const std::filesystem::path& dir) {
  std::error_code ec;
  if (std::filesystem::is_directory(dir, ec)) {
    return ok();
  }
  if (!std::filesystem::create_directories(dir, ec) || ec) {
    OTA_LOG_ERROR("创建目录失败: {} ({})", dir.string(), ec.message());
    return fail(ErrorCode::WRITE_FAILED);
  }
  return ok();
}

ExpectedVoid<ErrorCode> atomicWriteFile(const std::filesystem::path& path, const std::string& data) {
  if (auto ret = ensureDir(path.parent_path()); !ret.has_value()) {
    return ret;
  }

  const auto tmp_path = path.string() + ".tmp";
  const auto last_path = path.string() + ".last";

  if (writeRaw(tmp_path, data, /*sync=*/true) != ErrorCode::OK) {
    return fail(ErrorCode::WRITE_FAILED);
  }

  std::error_code ec;
  if (std::filesystem::exists(path, ec)) {
    std::filesystem::rename(path, last_path, ec);   // 留一份上一版
    if (ec) {
      OTA_LOG_ERROR("备份旧文件失败: {} ({})", path.string(), ec.message());
      return fail(ErrorCode::WRITE_FAILED);
    }
  }

  // rename 在同一文件系统内是原子的 -> 掉电不会写出半个文件
  std::filesystem::rename(tmp_path, path, ec);
  if (ec) {
    OTA_LOG_ERROR("rename 失败: {} ({})", path.string(), ec.message());
    return fail(ErrorCode::WRITE_FAILED);
  }
  return ok();
}

Expected<std::string, ErrorCode> readFile(const std::filesystem::path& path) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    return unexpected(ErrorCode::NOT_FOUND);
  }
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  return content;
}

std::vector<std::filesystem::path> findFilesWithExtension(const std::filesystem::path& dir,
                                                          const std::string& extension) {
  std::vector<std::filesystem::path> found;
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) {
    return found;   // 目录不存在：返回空，不抛异常
  }
  for (std::filesystem::recursive_directory_iterator it(dir, ec), end; it != end && !ec; it.increment(ec)) {
    if (it->is_regular_file(ec) && it->path().extension() == extension) {
      found.push_back(it->path());
    }
  }
  std::sort(found.begin(), found.end());
  return found;
}

bool dirHasAnyFile(const std::filesystem::path& dir) {
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) {
    return false;
  }
  for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
    if (entry.is_regular_file(ec)) {
      return true;
    }
  }
  return false;
}

void removeDirIfExists(const std::filesystem::path& dir) {
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

}  // namespace ota_mini::fs
