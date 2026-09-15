/*************************************************************
 * @FilePath: /Cpp/ota/syntax/10_filesystem_and_posix_io.cpp
 * @Brief: 语法点 10 —— std::filesystem + POSIX 裸 IO + 原子写文件
 *
 * 真实工程出处：
 *   - src/ota/state_machine/parsing_img.cpp
 *       std::error_code ec;
 *       std::filesystem::is_directory(dir, ec);
 *       std::filesystem::recursive_directory_iterator it(dir, ec), end;
 *       it->is_regular_file(ec) && it->path().extension() == ".bin"
 *   - src/utils/filesystem/file_operator.cpp
 *       fd_ = open(path.data(), flags, 0666);
 *       ftruncate(fd_, 0);  ::read()/::write() 循环;  fsync(fd_);  close(fd_);
 *       flags |= O_SYNC;
 *   - file_operator.cpp 的 updateFile()：**临时候 + rename 原子替换**
 *       写 tmp -> 老文件改名 .last -> tmp 改名成正式文件
 *       目的：掉电时文件要么是旧内容，要么是新内容，绝不会写一半
 *
 * 为什么工程里两种都要用？
 *   std::filesystem  : 跨平台、异常安全、易读（判断/遍历/创建目录）
 *   POSIX open/write : 能精确控制 O_SYNC/O_CREAT、能 fsync、性能可控（写状态文件）
 ***************************************************************/

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "mini_log.hpp"

namespace ota::utils::filesystem {

// ===========================================================================
// 1) std::filesystem 常用操作
// ===========================================================================
bool ensureDir(const std::filesystem::path& dir) {
  std::error_code ec;
  // 用带 error_code 的重载，避免异常（嵌入式常用）
  if (std::filesystem::exists(dir, ec)) {
    return std::filesystem::is_directory(dir, ec);
  }
  return std::filesystem::create_directories(dir, ec) && !ec;
}

// 递归遍历目录，收集指定后缀的文件（完全对应 parsing_img.cpp 的 hasUpgradeArtifacts）
std::vector<std::filesystem::path> findFilesWithExtension(const std::filesystem::path& dir,
                                                          const std::string& extension) {
  std::vector<std::filesystem::path> found;
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) {
    return found;
  }

  // 注意：迭代器构造也要传 ec，这样目录不存在时不会抛异常
  for (std::filesystem::recursive_directory_iterator it(dir, ec), end; it != end && !ec; it.increment(ec)) {
    if (it->is_regular_file(ec) && it->path().extension() == extension) {
      found.push_back(it->path());
    }
  }
  return found;
}

// ===========================================================================
// 2) POSIX 裸 IO 写文件（对应 FileOperator）
// ===========================================================================
struct PosixError {
  int err = 0;
  std::string message() const { return std::strerror(err); }
};

PosixError writeRaw(const std::string& path, std::string_view data, bool sync) {
  int flags = O_WRONLY | O_CREAT | O_TRUNC;   // 写 + 不存在则创建 + 截断
  if (sync) {
    flags |= O_SYNC;                          // 每次写都同步落盘（慢，但掉电安全）
  }
  const int fd = ::open(path.c_str(), flags, 0666);
  if (fd == -1) {
    return PosixError{errno};
  }

  std::size_t written_total = 0;
  while (written_total < data.size()) {
    // write 的返回值要检查：可能只写了一半（磁盘满 / 被信号打断）
    const ssize_t n = ::write(fd, data.data() + written_total, data.size() - written_total);
    if (n == -1) {
      const int e = errno;
      ::close(fd);
      return PosixError{e};
    }
    written_total += static_cast<std::size_t>(n);
  }

  if (!sync) {
    ::fsync(fd);   // 非 O_SYNC 模式下，手动 fsync 保证落盘
  }
  ::close(fd);     // 一定要关，否则 fd 泄漏
  return PosixError{};
}

// ===========================================================================
// 3) 原子写：临时文件 -> rename 替换
//    对应 FileOperator::updateFile
// ===========================================================================
bool atomicWriteFile(const std::string& path, std::string_view data) {
  const std::string tmp_path = path + ".tmp";
  const std::string last_path = path + ".last";

  if (auto e = writeRaw(tmp_path, data, /*sync=*/true); e.err != 0) {
    MINI_ERROR("写临时文件失败: {} ({})", tmp_path, e.message());
    return false;
  }

  std::error_code ec;
  if (std::filesystem::exists(path, ec)) {
    std::filesystem::rename(path, last_path, ec);   // 保留上一版
    if (ec) {
      MINI_ERROR("备份旧文件失败: {}", ec.message());
      return false;
    }
  }

  // rename 在同一文件系统内是原子操作 —— 这是"掉电安全"的关键
  std::filesystem::rename(tmp_path, path, ec);
  if (ec) {
    MINI_ERROR("rename 失败: {}", ec.message());
    return false;
  }
  return true;
}

}  // namespace ota::utils::filesystem

int main() {
  using namespace ota::utils::filesystem;
  printTitle("10 std::filesystem + POSIX IO + 原子写");
  printSourceHint("src/utils/filesystem/file_operator.cpp, src/ota/state_machine/parsing_img.cpp");

  // 用 /tmp 下的隔离目录，不污染系统
  const std::filesystem::path root = "/tmp/ota_fs_demo";

  printStep("1) 创建目录树");
  std::error_code ec;
  std::filesystem::remove_all(root, ec);   // 先清干净，方便反复运行
  MINI_INFO("create_directories 返回: {}", ensureDir(root / "img" / "nx_mcu"));
  MINI_INFO("create_directories 返回: {}", ensureDir(root / "img" / "rk_motor"));
  MINI_INFO("create_directories 返回: {}", ensureDir(root / "img" / "nx_self"));
  MINI_INFO("目录现在存在? {}", std::filesystem::is_directory(root / "img", ec));

  printStep("2) 造几个假固件文件（对应升级包 img 目录）");
  std::vector<std::pair<std::string, std::string>> files = {
      {"img/nx_mcu/mcu_v1.0.3.bin", "fake mcu firmware"},
      {"img/rk_motor/motor_v2.1.0.bin", "fake motor firmware"},
      {"img/rk_motor/readme.txt", "this should be filtered out"},
      {"img/nx_self/app_v3.0.0.deb", "fake deb package"},
      {"img/empty_dir_note.txt", "placeholder"},
  };
  for (const auto& [rel, content] : files) {
    const auto p = root / rel;
    std::ofstream ofs(p, std::ios::binary);
    ofs << content;
    MINI_INFO("写入 {}", p.string());
  }

  printStep("3) 递归查找 .bin（对应 hasUpgradeArtifacts）");
  for (const auto& f : findFilesWithExtension(root / "img" / "nx_mcu", ".bin")) {
    MINI_INFO("nx_mcu 里的 .bin  : {}", f.string());
  }
  for (const auto& f : findFilesWithExtension(root / "img" / "rk_motor", ".bin")) {
    MINI_INFO("rk_motor 里的 .bin: {} (readme.txt 被后缀过滤掉)", f.string());
  }
  MINI_INFO("nx_self 里的 .bin : {} 个（这里只有 .deb）", findFilesWithExtension(root / "img" / "nx_self", ".bin").size());
  MINI_INFO("不存在的目录返回空 : {} 个（不会抛异常）", findFilesWithExtension(root / "img" / "not_exist", ".bin").size());

  printStep("4) POSIX 裸 IO 写文件");
  const std::string state_file = (root / "ota_work" / "current_ota_state").string();
  ensureDir(root / "ota_work");
  if (auto e = writeRaw(state_file, "3", true); e.err != 0) {
    MINI_ERROR("写失败: {}", e.message());
  } else {
    MINI_INFO("O_SYNC 写入成功: {} -> \"3\"", state_file);
  }

  printStep("5) 原子写：第一次写 + 第二次覆盖（会留下 .last 备份）");
  MINI_INFO("第一次原子写: 内容=\"2\" -> {}", atomicWriteFile(state_file, "2"));
  MINI_INFO("第二次原子写: 内容=\"3\" -> {}", atomicWriteFile(state_file, "3"));

  const auto readAll = [](const std::string& p) {
    std::ifstream ifs(p);
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  };
  MINI_INFO("正式文件内容 : \"{}\"", readAll(state_file));
  MINI_INFO("备份文件内容 : \"{}\"  (上一版，掉电时可回滚)", readAll(state_file + ".last"));

  printStep("6) 目录内容一览（递归）");
  for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
    const bool is_dir = entry.is_directory(ec);
    MINI_INFO("{} {}", is_dir ? "[D]" : "[F]", entry.path().string());
  }

  printStep("7) 文件大小 / 磁盘剩余空间");
  MINI_INFO("state_file 大小 : {} 字节", std::filesystem::file_size(state_file, ec));
  const auto space = std::filesystem::space(root, ec);
  MINI_INFO("/tmp 可用空间   : {} MiB / 总 {} MiB", space.available / 1024 / 1024, space.capacity / 1024 / 1024);

  std::filesystem::remove_all(root, ec);
  MINI_INFO("已清理演示目录 {}", root.string());

  printStep("小结");
  std::cout << R"(
    - std::filesystem 的一切操作都有两个版本：
        抛异常版      : exists(p)
        返回 error_code 版: exists(p, ec)     ← 嵌入式/实时场景首选
    - recursive_directory_iterator(dir, ec) 遍历时，每一步 increment(ec) 也别漏 ec
    - POSIX open 标志：O_WRONLY | O_CREAT | O_TRUNC | O_SYNC；权限 0666
    - write() 可能"只写一半"，必须循环写直到写完；返回 -1 要立刻 close 再返回
    - fd 一定要 close，否则泄漏（工程里用 FileOperator 的析构函数保证）
    - 原子写三步：写 tmp(O_SYNC) -> 老文件 rename 成 .last -> tmp rename 成正式名
    - rename 在同一文件系统内是原子的；这是"掉电不写坏配置"的标准做法
    - std::filesystem::space() 查磁盘剩余空间，OTA 前检查用得上
)";
  return 0;
}
