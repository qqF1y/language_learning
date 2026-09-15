# ota_mini —— 综合性小例程

把 `syntax/` 里 16 个语法点**全部用上**的一个完整迷你 OTA 升级程序。

它模仿真实工程 `humanoid_ota` 的架构：**服务回调线程置标志 → 条件变量唤醒 → 单线程状态机
逐状态推进 → 状态类析构里落盘并发 topic → 线程池并行升级设备**。

代码量约 1600 行，纯标准库（C++20），可编译可运行，不需要 ROS2 / LCM / curl / spdlog。

---

## 1. 编译运行

```bash
cd /home/qiurangfei/PRJ/language_learning/Cpp/ota
cmake -S examples/ota_mini -B build_ota_mini -DCMAKE_BUILD_TYPE=Release
cmake --build build_ota_mini -j

# ① 无人值守跑完整流程（推荐先跑这个）
./build_ota_mini/ota_mini --demo

# ② 交互式，自己敲命令
./build_ota_mini/ota_mini

# ③ 指定配置文件 / 只看警告
./build_ota_mini/ota_mini --config configures/ota_mini.yaml --quiet
```

> 运行状态写在 `/tmp/ota_mini/work` 下（状态文件、下载目录、版本号 json）。
> 想重来一遍：`rm -rf /tmp/ota_mini`。

---

## 2. 交互式命令（等价于 ros2 service / topic）

| 命令 | 对应真实工程 |
| --- | --- |
| `local ALL` | `ros2 service call /ota_local common_msgs/srv/BaseString "{data: "ALL"}"` |
| `local RK_MOTOR` | 同上，`data: "RK_MOTOR"` |
| `local RK_MCU` | 同上，`data: "RK_MCU"` |
| `local RK_SELF` | 同上，`data: "RK_SELF"` |
| `local NX_SELF` | 同上，`data: "NX_SELF"` |
| `local xxx.bin` | 同上，`data: "xxx.bin"`（NX 直连 MCU 的单文件升级） |
| `cloud v1.0 https://.../img.zip` | `ros2 service call /ota_eame common_msgs/srv/OtaCmd "{version: "v1.0", url: "https://..."}"` |
| `reset` | `ros2 service call /ota_reset common_msgs/srv/SetInt8 "{data: 0}"` |
| `status` | `ros2 topic echo /ota_status` |
| `battery 10` | `/battery_state` 话题消息 |
| `hang 1` | `/manager/robot_hanged` 话题消息 |

`--demo` 模式会自动跑：

1. `local ALL` —— 本地整包升级（4 个任务，含一个被跳过的缺失模块）
2. `cloud v9.9.9 <url>` —— 云端升级（先下载，再走同样的任务流程）
3. 低电量保护 —— 把电量设为 10%，服务应拒绝
4. `local RK_SELF` —— 只升级 RK3588 自身
5. `local mcu_v1.0.4.bin` —— 单文件升级

最后打印 `/ota_status` 和落盘的 `mcu_version.json`。

**想看到失败路径**：在 `configures/ota_mini.yaml` 里放开

```yaml
device_fail_ids:
  - 2
```

再跑 `--demo`，第 2 号设备会升级失败，流程进入 `FAILED`，demo 会自动发一次 `/ota_reset`
把它复位回 `IDLE`。

---

## 3. 文件 → 真实工程 对应表

| 本目录文件 | 真实工程对应 | 用到的语法点 |
| --- | --- | --- |
| `src/log.hpp` | `utils/logger`(mjrrt/spdlog) | 变参模板、折叠表达式、宏、`string_view`、函数内 static |
| `src/result.hpp` | `utils/*/error.hpp` + `tl::expected` | X-Macro、`Expected<T,E>`、`Unexpected`、结构化绑定 |
| `src/config.hpp` / `.cpp` | `utils/configures` | 类内默认值、`unordered_map`、字符串切分、`if` 初始化语句 |
| `src/thread_utils.hpp` | `utils/waiter` + `third_party/thread_pool` | `condition_variable`、`unique_lock`、`packaged_task`、`future`、模板转发 |
| `src/fs_utils.hpp` / `.cpp` | `utils/filesystem` | `std::filesystem`（含 `error_code` 版本）、POSIX `open/write/fsync/rename`、原子写 |
| `src/shell_command.hpp` / `.cpp` | `utils/shell_command` | `popen/pclose`、`sigaction(SIGPIPE)`、`unique_ptr` + 自定义删除器、`WIFEXITED` |
| `src/state_machine.hpp` / `.cpp` | `state_machine/state_machine.hpp|.cpp` | X-Macro、`atomic<enum>`、转移表、抽象基类、**析构里推进状态** |
| `src/states.hpp` / `.cpp` | `state_machine/{init,idle,download,...}.cpp` | `override`、`final`、lambda、线程池、`Expected` |
| `src/ota_node.hpp` / `.cpp` | `ota/ota.hpp` / `ota.cpp` | 双线程装配、服务/话题模拟、`unordered_map<Enum, function<...>>` 分发、`try/catch + terminate` |
| `src/main.cpp` | `main.cpp` + 用户手敲的 ros2 命令 | 命令行解析、`std::function` 谓词、轮询等待 |
| `configures/ota_mini.yaml` | `configures/a2409.yaml` | 配置驱动行为 |

---

## 4. 状态机全景

```
                        ┌─────────────────────────────────────────┐
                        │                                         │
   [*] ──> init ──> idle │ <── failed <──── (任何失败)             │
             │      ▲    │        ▲                               │
             │      │    │        │                               │
             │      │    │        └── check_prerequisites ──> download
             │      │    │                    │                    │
             │      │    │                    └────> parsing_img <──┘
             │      │    │                              │
             │      │    │                              ▼
             │      │    │                       check_ota_task
             │      │    │            ┌───────────────┼───────────────┐
             │      │    │            ▼               ▼               ▼
             │      │    │      send_img_data    nx_self    rk_send_ota_cmd
             │      │    │            │               │               │
             │      │    │            └───────────────┴───────────────┘
             │      │    │                            │
             │      │    │                            ▼
             │      │    └────(还有任务)─────────── success
             │      │                                 │(全部完成)
             │      └────────────(不重启)───────── reboot ──> pub_hard_reboot
             │                                                    │
             └────────────────────────────────────────────────────┘
```

`StateMachineFunction` 的设计与真实工程一致：

```cpp
class StateMachineFunction {
 public:
  explicit StateMachineFunction(StateMachineType type);
  virtual void run() = 0;                 // 子类只实现这个
  virtual ~StateMachineFunction();        // ← 状态推进/落盘/发 topic 全在这里！
};

// 子类长这样：
class Download final : public StateMachineFunction {
 public:
  Download() : StateMachineFunction(StateMachineType::DOWNLOAD) {}
  void run() override {
    // ... 干活 ...
    setNextStateMachineType(StateMachineType::PARSING_IMG, ota_state::DOWNLOAD_SUCCESS, "下载完成");
  }   // ← 出作用域，基类析构统一收尾
};
```

**非法转移会 `std::terminate()`**，与 boost::sml 的行为一致（真实工程因为这个崩过，
详见 `docs/01_工程总结.md` 第 11 节）。

---

## 5. 数据流（一次 `local ALL` 的完整链路）

```
用户敲 local ALL
   │
   ▼  OtaNode::srvOtaLocalCmd            （模拟 ROS2 回调线程）
   │    ├─ 检查电量
   │    └─ StateMachineData::setIsReceiveTask()
   │         ├─ lock_guard 保护 4 个字段
   │         └─ idle_task_waiter.notify()  ←────────────┐
   │                                                    │
   ▼  Idle::run() 从 wait() 醒来 ────────────────────────┘
   │    └─ 升级类型/URL/版本号 原子落盘
   ▼
  CheckPrerequisites::run()  电量 + 挂起检查 + 创建线程池 + TTS
   ▼
  ParsingImg::run()          扫描 img 目录，按 upgrade_sequence 生成任务列表
   ▼
  CheckOtaTask::run()        取 task_type_list[index] 分发
   ▼
  SendImgData::run()         thread_pool->enqueue(每台设备) → future.get() 汇总
   ▼
  Success::run()             index++ 落盘；还有任务回 CheckOtaTask，否则进 Reboot
   ▼
  Reboot::run()              写 mcu_version.json；按配置软重启/硬重启/不重启
   ▼
  Idle::run()                回到等任务，可以再次升级
```

---

## 6. 和真实工程的差异（刻意简化）

| 真实工程 | 本例子 | 说明 |
| --- | --- | --- |
| boost::sml 编译期状态机 | 手写 `(from,to)` 转移表 + 校验 | 语义一致，但不依赖第三方库；例程 13 讲了等价的分发表写法 |
| ROS2 service/topic/LCM | 普通函数调用 | 线程模型完全一致（回调只置标志 + notify） |
| libcurl 真下载 | 循环 sleep 模拟 + 进度回调 | 保留了"进度回调里发 topic"这个关键结构 |
| 真实串口升级协议 | sleep 模拟 + 写日志文件 | 保留了线程池并行 + `future` 聚合 |
| nlohmann::json | 手拼 JSON 字符串 | 只为了不引入依赖 |
| reflect-cpp 解析 yaml | 手写极简 yaml 子集解析 | 结构体定义风格保持一致 |
| EtherCAT 电机升级 | 用假的电机固件文件模拟 | 保留了"电机数量校验"这一步 |

---

## 7. 建议的动手实验

1. **看正常运行**：`--demo`，在日志里找状态流转的 `>>> 进入状态` / `<<< 结束状态` 配对。
2. **看失败 + 复位**：放开 `device_fail_ids`，观察 `FAILED` 状态如何阻塞等 `/ota_reset`。
3. **看断点续升级**：升级进行到一半按 `Ctrl-C`，然后重新启动程序。
   `Init::run()` 会读 `current_ota_state` / `current_ota_task_list` / `current_ota_task_index`
   把流程接回去（现代码默认会走完，可以在 `Success` 里加个 sleep 方便中断）。
4. **看死锁/卡死的坑**：把 `Idle::run()` 里那个 `wait(condition, 5s)` 换成
   `while (!flag) sleep(100ms)` 轮询，对比 CPU 占用和响应延迟。
5. **看非法转移**：在 `CheckOtaTask::run()` 里加一句
   `setNextStateMachineType(StateMachineType::REBOOT, ota_state::FAIL, "xx")`，
   因为 `check_ota_task -> reboot` 不在转移表里，程序会立刻报错并 `terminate`。
6. **看线程池并行收益**：把 `config.yaml` 的 `thread_pool_size` 改成 1，对比
   `SendImgData` 阶段的耗时（日志时间戳）。
7. **看原子写**：升级过程中 `cat /tmp/ota_mini/work/current_ota_state`，
   再看看旁边的 `.last` 备份文件。
