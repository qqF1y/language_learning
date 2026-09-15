/*************************************************************
 * @FilePath: /Cpp/ota/syntax/04_raii_smart_pointer.cpp
 * @Brief: 语法点 04 —— RAII 与智能指针的自定义删除器
 *
 * 真实工程出处：
 *   - src/utils/shell_command/shell_command.cpp
 *       std::unique_ptr<FILE, decltype(pipe_close)> pipe(nullptr, pipe_close);
 *       // popen 必须配 pclose，否则僵尸进程；用 RAII 保证异常路径也会关
 *   - src/utils/curl/curl.cpp
 *       std::unique_ptr<CURL, std::function<void(CURL*)>> curl_ptr(curl, [](CURL* c){...});
 *   - src/utils/filesystem/file_operator.hpp
 *       class FileOperator { int fd_; ~FileOperator(){ if (fd_!=-1) close(fd_); } };
 *
 * 三个要点：
 *   1. unique_ptr 默认删除器是 delete，管不了 FILE* 和 int fd 这类 C 资源
 *   2. 传函数对象当删除器：decltype(deleter) 或 std::function<...>
 *   3. 构造失败时（比如 popen 返回 nullptr）仍要保证不误删
 ***************************************************************/

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "mini_log.hpp"

namespace ota::utils {

// ===========================================================================
// 1) 手写 RAII 类：对应真实工程的 FileOperator
//    核心：构造函数获取资源，析构函数释放资源，拷贝要禁掉（否则 double free）
// ===========================================================================
class FileGuard {
 public:
  explicit FileGuard(const std::string& path) {
    file_ = std::fopen(path.c_str(), "w");
    if (file_ == nullptr) {
      MINI_ERROR("fopen 失败: {} ({})", path, std::strerror(errno));
    }
  }

  ~FileGuard() {
    if (file_ != nullptr) {
      std::fclose(file_);
      MINI_INFO("析构：已自动 fclose，relpath={}", path_hint_);
    }
  }

  // Rule of Five：管资源就要禁拷贝（否则两个对象指向同一 FILE*）
  FileGuard(const FileGuard&) = delete;
  FileGuard& operator=(const FileGuard&) = delete;

  // 允许移动：转移所有权，源对象置空
  FileGuard(FileGuard&& other) noexcept : file_(other.file_), path_hint_(std::move(other.path_hint_)) {
    other.file_ = nullptr;
  }

  bool good() const { return file_ != nullptr; }

  void setHint(std::string hint) { path_hint_ = std::move(hint); }

 private:
  FILE* file_ = nullptr;
  std::string path_hint_;
};

// ===========================================================================
// 2) 带自定义删除器的 unique_ptr
//    删除器类型必须作为模板参数；用 decltype(lambda) 是最省事的写法
// ===========================================================================
struct FcloseDeleter {
  void operator()(FILE* f) const {
    if (f != nullptr) {
      std::fclose(f);
      MINI_INFO("自定义删除器：fclose 被调用");
    }
  }
};

using UniqueFile = std::unique_ptr<FILE, FcloseDeleter>;

UniqueFile openFile(const std::string& path, const char* mode) {
  return UniqueFile(std::fopen(path.c_str(), mode));  // 失败(返回 nullptr)也安全
}

// ===========================================================================
// 3) lambda 删除器 + decltype：最贴近工程 curl.cpp 的写法
//    工程里为了能用 std::function 做类型擦除，显式写了 std::function<void(CURL*)>
// ===========================================================================
using CurlHandle = std::unique_ptr<void, std::function<void(void*)>>;

CurlHandle fakeCurlInit() {
  void* handle = std::malloc(16);  // 假装是 curl_easy_init()
  MINI_INFO("fakeCurlInit: 分配了一个假句柄 {}", handle);
  return CurlHandle(handle, [](void* h) {
    if (h != nullptr) {
      std::free(h);
      MINI_INFO("lambda 删除器：资源已释放");
    }
  });
}

}  // namespace ota::utils

// ===========================================================================
// 4) 演示"异常安全"：中途抛异常，资源依然被释放
// ===========================================================================
static void doWorkWithException() {
  ota::utils::FileGuard guard("/tmp/ota_raii_demo.txt");
  guard.setHint("ota_raii_demo.txt");
  MINI_INFO("工作中…… 马上要抛异常了");
  throw std::runtime_error("模拟下载失败");
  // 注意：这里不需要写 fclose，栈展开时 guard 的析构函数会被调用
}

int main() {
  using namespace ota::utils;
  printTitle("04 RAII / unique_ptr 自定义删除器");
  printSourceHint("src/utils/shell_command/shell_command.cpp, src/utils/curl/curl.cpp");

  printStep("1) 手写 RAII 类");
  {
    FileGuard guard("/tmp/ota_raii_demo.txt");
    guard.setHint("ota_raii_demo.txt");
    MINI_INFO("guard.good() = {}", guard.good());
    // 离开作用域自动关闭
  }
  MINI_INFO("作用域已结束");

  printStep("2) unique_ptr + 函数对象删除器");
  {
    UniqueFile f = openFile("/tmp/ota_raii_unique.txt", "w");
    if (f) {
      std::fputs("hello ota\n", f.get());
      MINI_INFO("写入了数据, 句柄地址 = {}", static_cast<void*>(f.get()));
    }
  }  // <- 这里 FcloseDeleter 被调用
  MINI_INFO("unique_ptr 作用域已结束");

  printStep("3) unique_ptr + lambda 删除器（对应 curl.cpp）");
  {
    auto handle = fakeCurlInit();
    MINI_INFO("使用句柄……");
  }  // <- lambda 删除器被调用
  MINI_INFO("lambda 删除器作用域已结束");

  printStep("4) 构造失败也不误删：nullptr 交给删除器处理");
  {
    UniqueFile bad = openFile("/no/such/dir/xxx.txt", "w");
    MINI_INFO("打开失败时 unique_ptr 为 nullptr? {}", bad == nullptr);
  }  // 删除器会收到 nullptr，需要自己判空
  MINI_INFO("失败路径正常退出，无崩溃");

  printStep("5) 异常安全：抛异常也要释放资源");
  try {
    doWorkWithException();
  } catch (const std::exception& e) {
    MINI_ERROR("捕获到异常: {}", e.what());
  }
  MINI_INFO("异常已捕获，文件已由 RAII 关闭");

  printStep("6) 所有权转移：move 而不是 copy");
  {
    FileGuard a("/tmp/ota_raii_move_a.txt");
    a.setHint("A");
    FileGuard b = std::move(a);  // a 变成空壳
    MINI_INFO("move 后 b.good() = {}", b.good());
    // 注意：a 依然会被析构，但因为它内部指针已置空，不会 double free
  }
  MINI_INFO("移动出的对象析构完毕");

  printStep("小结");
  std::cout << R"(
    - RAII = 构造拿资源 + 析构放资源，异常/提前 return 都不会泄漏
    - unique_ptr<T, Deleter>：Deleter 是模板参数，可以是函数对象或 lambda
    - 用 decltype(lambda) 或 std::function<void(T*)> 声明删除器类型
    - 管资源的类必须 delete 拷贝构造/赋值（Rule of Five），否则 double free
    - 删除器必须能处理 nullptr（构造失败的情况）
    - unique_ptr 独占所有权，要转移用 std::move，不要用 std::shared_ptr 图省事
)";
  return 0;
}
