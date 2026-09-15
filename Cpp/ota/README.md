# humanoid_ota 工程学习笔记

本目录是对 `/home/qiurangfei/PRJ/ota/humanoid_ota`（人形机器人 OTA 升级工程）的**逐语法拆解 + 可运行例程**。

所有例程**只依赖标准库（C++20）**，不依赖 ROS2 / LCM / fmt / spdlog / boost::sml，
可以在本机直接 `cmake && make` 编译运行，用来对着真实工程源码学习语法。

## 目录结构

```
Cpp/ota/
├── README.md                       # 本文件
├── docs/
│   ├── 01_工程总结.md               # 工程架构 / 分层 / 数据流 / 状态机全景
│   └── 02_语法知识点索引.md         # 语法点 → 真实源码位置 → 例程文件 对照表
├── syntax/                         # ① 按语法点拆分的独立小例程（每个都能单独跑）
│   ├── CMakeLists.txt
│   ├── mini_log.hpp                # 公共迷你日志/格式化工具（变参模板教学）
│   ├── xmacro_list.hpp             # 公共 X-Macro 列表（供多个例程复用）
│   ├── 01_namespace_headers.cpp
│   ├── 02_xmacro_enum.cpp
│   ├── 03_enum_class_atomic.cpp
│   ├── 04_raii_smart_pointer.cpp
│   ├── 05_lambda_function_callback.cpp
│   ├── 06_thread_atomic_waiter.cpp
│   ├── 07_mutex_thread_safe_state.cpp
│   ├── 08_optional_expected.cpp
│   ├── 09_string_view_format.cpp
│   ├── 10_filesystem_and_posix_io.cpp
│   ├── 11_template_specialization.cpp
│   ├── 12_abstract_polymorphism.cpp
│   ├── 13_function_dispatch_map.cpp
│   ├── 14_popen_signal_pipe.cpp
│   ├── 15_thread_pool_future.cpp
│   └── 16_modern_cpp_misc.cpp
└── examples/
    └── ota_mini/                   # ② 综合性小例程：一个完整可运行的迷你 OTA 状态机
        ├── CMakeLists.txt
        ├── README.md               # 例程说明 + 运行方法 + 与真实工程的对应关系
        ├── configures/
        │   └── ota_mini.yaml       # 配置文件（对应真实工程 configures/a2409.yaml）
        └── src/
            ├── log.hpp             # 迷你日志（变参模板 / 折叠表达式）
            ├── result.hpp          # ErrorCode(X-Macro) + Expected<T,E>
            ├── config.hpp/.cpp     # 配置结构体 + 极简 yaml 解析
            ├── thread_utils.hpp    # Waiter + ThreadPool
            ├── fs_utils.hpp/.cpp   # std::filesystem + POSIX 原子写
            ├── shell_command.hpp/.cpp  # popen/pclose + SIGPIPE
            ├── state_machine.hpp/.cpp  # 状态枚举 / 转移表 / 状态基类
            ├── states.hpp/.cpp     # 13 个具体状态
            ├── ota_node.hpp/.cpp   # 节点装配（模拟 ROS2 服务/话题 + 双线程）
            └── main.cpp            # 入口 + 交互式命令行
```

## 编译运行

### 方式一：一条命令编译全部（推荐）

```bash
cd /home/qiurangfei/PRJ/language_learning/Cpp/ota
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

./build/01_namespace_headers          # 逐个运行语法例程
./build/16_modern_cpp_misc
./build/ota_mini --demo               # 综合例程：自动跑完整 OTA 流程
./build/ota_mini                      # 综合例程：交互式
```

### 方式二：单独编译

```bash
# 只编译 16 个语法例程
cmake -S syntax -B build_syntax && cmake --build build_syntax -j

# 一次运行全部语法例程（看输出）
cmake --build build_syntax --target run_all_syntax

# 只编译综合例程
cmake -S examples/ota_mini -B build_ota_mini && cmake --build build_ota_mini -j
./build_ota_mini/ota_mini --demo
# 或
cmake --build build_ota_mini --target run_demo
```

> `ota_mini` 的默认配置文件路径是**编译期写死的绝对路径**（指向源码目录下的
> `configures/ota_mini.yaml`），所以从任何目录运行都能找到配置。
> 运行产生的状态文件在 `/tmp/ota_mini/work`，想重来一遍就 `rm -rf /tmp/ota_mini`。

## 阅读顺序建议

1. 先看 `docs/01_工程总结.md`，建立"这个工程在干什么"的整体认知。
2. 按 `docs/02_语法知识点索引.md` 的表格，挑一个语法点，跳到 `syntax/` 里跑对应例程。
3. 最后读 `examples/ota_mini/`，这是把前面所有语法点拼起来的一个"麻雀虽小五脏俱全"的 OTA 状态机。
   它的 `README.md` 里有「文件 → 真实工程」对照表和一组建议的动手实验。
