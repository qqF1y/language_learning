/*************************************************************
 * @FilePath: /Cpp/ota/syntax/05_lambda_function_callback.cpp
 * @Brief: 语法点 05 —— lambda 捕获、std::function 类型擦除、C 回调桥接
 *
 * 真实工程出处：
 *   - src/ota/state_machine/download.cpp
 *       auto progress_callback = [this](curl_off_t total, curl_off_t now, ...) { ... };
 *       auto write_callback    = [&file_operator](void* ptr, size_t size, size_t nmemb, void*) -> size_t {
 *         auto result = file_operator.append(static_cast<char*>(ptr), size * nmemb);
 *         return result != OK ? 0 : size * nmemb;   // 返回 0 = 通知 curl 中断
 *       };
 *       utils::curl::Curl::downloadFile(url, progress_callback, write_callback);
 *   - src/utils/curl/curl.cpp
 *       // C API 只能传裸指针，于是把 std::function 装箱成 void*，在 static 函数里拆箱
 *       static int progressCallback(void* ptr, curl_off_t a, curl_off_t b, curl_off_t c, curl_off_t d) {
 *         auto cb = *static_cast<std::function<void(...)>*>(ptr);
 *         cb(a, b, c, d);
 *         return 0;
 *       }
 ***************************************************************/

#include <cstddef>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "mini_log.hpp"

namespace ota::utils {

// ---------------------------------------------------------------------------
// 1) 模仿 libcurl 的 C 风格函数指针类型
//    C API 的限制：只能传裸函数指针 + 一个 void* userdata
// ---------------------------------------------------------------------------
using ProgressFn = int (*)(void* userdata, long long total, long long now);
using WriteFn = std::size_t (*)(void* userdata, void* ptr, std::size_t size, std::size_t nmemb);

// ---------------------------------------------------------------------------
// 2) C++ 侧的封装：接受 std::function（可以装任何可调用对象）
// ---------------------------------------------------------------------------
class Downloader {
 public:
  // 进度回调：4 个参数，无返回值
  using ProgressCallback = std::function<void(long long total, long long now)>;
  // 写回调：返回实际"消费"的字节数，返回 0 表示让 curl 中断下载
  using WriteCallback = std::function<std::size_t(void* ptr, std::size_t size, std::size_t nmemb, void* userdata)>;

  void setProgressCallback(ProgressCallback cb) { progress_cb_ = std::move(cb); }
  void setWriteCallback(WriteCallback cb) { write_cb_ = std::move(cb); }

  // 模拟 curl_easy_perform：分片"下载"
  int perform(const std::string& url) {
    constexpr long long kTotal = 100;
    std::vector<char> chunk(16, 'x');
    for (long long now = 20; now <= kTotal; now += 20) {
      // 进度回调
      if (progress_cb_) {
        progress_cb_(kTotal, now);
      }
      // 写回调：注意最后一个参数是 userdata（真实工程里传的是 &write_callback 本身）
      if (write_cb_) {
        const auto written = write_cb_(chunk.data(), 1, chunk.size(), nullptr);
        if (written == 0) {
          MINI_ERROR("写回调返回 0，模拟下载中断: {}", url);
          return -1;
        }
      }
    }
    return 0;
  }

 private:
  ProgressCallback progress_cb_;
  WriteCallback write_cb_;
};

}  // namespace ota::utils

// ===========================================================================
// 3) C 回调 -> C++ 闭包 的桥接（真实工程 curl.cpp 的核心技巧）
//    libcurl 只接受静态函数指针，所以需要：
//      静态函数(裸指针) --拆箱--> std::function --调用--> 用户 lambda
// ===========================================================================
namespace bridge {

// 把 C 风格函数指针类型引进本命名空间
using ota::utils::ProgressFn;
using ota::utils::WriteFn;

// 静态函数作回调：签名必须和 C API 完全一致
static int progressTrampoline(void* userdata, long long total, long long now) {
  // userdata 里放的是 std::function 对象的地址
  auto* cb = static_cast<std::function<void(long long, long long)>*>(userdata);
  (*cb)(total, now);
  return 0;  // 返回非 0 会让 curl 中止
}

static std::size_t writeTrampoline(void* userdata, void* ptr, std::size_t size, std::size_t nmemb) {
  auto* cb = static_cast<std::function<std::size_t(void*, std::size_t, std::size_t)>*>(userdata);
  return (*cb)(ptr, size, nmemb);
}

// 模拟 curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressTrampoline)
//              curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &callback)
void cApiPerform(ProgressFn prog, WriteFn write, void* prog_ud, void* write_ud) {
  for (long long now = 25; now <= 100; now += 25) {
    if (prog) prog(prog_ud, 100, now);
    char data[8] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
    if (write) write(write_ud, data, 1, sizeof(data));
  }
}

}  // namespace bridge

int main() {
  using namespace ota::utils;
  printTitle("05 lambda / std::function / C 回调桥接");
  printSourceHint("src/ota/state_machine/download.cpp, src/utils/curl/curl.cpp");

  printStep("1) lambda 的各种捕获方式");
  int counter = 100;
  std::string prefix = "OTA";

  auto by_value = [counter] { return counter; };                       // 拷贝一份，之后外部改动看不到
  auto by_ref = [&counter] { counter += 1; return counter; };          // 引用，改动反映到外部
  auto by_all_ref = [&] { return prefix + std::to_string(counter); };  // [&] 捕获所有用到的变量
  auto init_capture = [n = counter * 2] { return n; };                 // C++14 初始化捕获：造个新变量
  auto mutable_lambda = [counter]() mutable { return ++counter; };     // 按值捕获 + mutable 才能改副本

  MINI_INFO("按值捕获        : {}", by_value());
  MINI_INFO("按引用捕获      : {}", by_ref());
  MINI_INFO("外部 counter 变成: {}", counter);
  MINI_INFO("[&] 全按引用    : {}", by_all_ref());
  MINI_INFO("初始化捕获 n=c*2: {}", init_capture());
  MINI_INFO("mutable 第一次  : {}", mutable_lambda());
  MINI_INFO("mutable 第二次  : {} (改的是内部副本，外部 counter 仍是 {})", mutable_lambda(), counter);

  printStep("2) 泛型 lambda (auto 参数) —— 相当于模板函数对象");
  auto genericPrint = [](const auto& v) { MINI_INFO("泛型 lambda 收到: {}", v); };
  genericPrint(42);
  genericPrint(std::string("hello"));
  genericPrint(3.14);

  printStep("3) std::function 类型擦除：把 lambda 存进成员变量");
  Downloader downloader;
  long long last_progress = -1;
  std::string downloaded;

  // 捕获 this 的 lambda：真实工程 progress_callback 就是这么写的
  downloader.setProgressCallback([&last_progress](long long total, long long now) {
    const auto pct = static_cast<int>(now * 100 / total);
    if (pct != last_progress) {
      last_progress = pct;
      MINI_INFO("下载进度: {}%", pct);
    }
  });

  // 捕获引用的 lambda：真实工程 write_callback 捕获了 file_operator
  downloader.setWriteCallback([&downloaded](void* ptr, std::size_t size, std::size_t nmemb, void*) -> std::size_t {
    downloaded.append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
  });

  MINI_INFO("perform 返回: {}", downloader.perform("https://example.com/img.zip"));
  MINI_INFO("已下载字节数: {}", downloaded.size());

  printStep("4) 写回调返回 0 = 主动中断下载（错误传播技巧）");
  Downloader failing;
  failing.setWriteCallback([](void*, std::size_t, std::size_t, void*) -> std::size_t {
    return 0;  // 磁盘满 / 写失败时就这么干
  });
  MINI_INFO("perform 返回: {} (非 0 表示失败)", failing.perform("https://example.com/img.zip"));

  printStep("5) C 回调桥接：静态蹦床函数 + void* userdata");
  std::function<void(long long, long long)> progress = [](long long total, long long now) {
    MINI_INFO("  桥接后的进度回调: {}/{}", now, total);
  };
  std::string bridged;
  std::function<std::size_t(void*, std::size_t, std::size_t)> writer =
      [&bridged](void* ptr, std::size_t size, std::size_t nmemb) -> std::size_t {
    bridged.append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
  };

  // 关键：把 std::function 的地址当 userdata 传下去（必须保证生命周期覆盖调用期！）
  bridge::cApiPerform(bridge::progressTrampoline, bridge::writeTrampoline, &progress, &writer);
  MINI_INFO("桥接路径收到 {} 字节: {}", bridged.size(), std::string(bridged.substr(0, 16)) + "...");

  printStep("小结");
  std::cout << R"(
    - [x] 按值捕获（默认，安全） / [&x] 按引用（要保证生命周期） / [&] [=] 全捕获
    - [n = expr] 初始化捕获：C++14 起可用，能把 move-only 对象搬进 lambda
    - mutable：按值捕获的副本可以修改；不加 mutable 是 const 的
    - std::function<Sig>：类型擦除，代价是一次间接调用 + 可能堆分配
    - C API 桥接三件套：
        1. static 蹦床函数，签名与 C 函数指针一致
        2. void* userdata 传 C++ 对象地址
        3. 蹦床里 static_cast 回来再调用
    - 回调返回 0 / 非 0 常被 C API 用作"中断信号"，要看清文档
)";
  return 0;
}
