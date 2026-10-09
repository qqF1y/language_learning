# C 语言练习

三块内容，先学语法，再看真实工程里 C 是怎么用的。

```bash
cd C
make run          # 跑 soc_boot 启动 demo（推荐先看这个）
make run-basics   # 依次跑 18 个基础例程
make run-driver   # 跑内核驱动原理 demo（用户态仿真版，不需要 root）
make              # 全部编译
make clean
```

## 目录

| 目录 | 内容 |
|---|---|
| [`basics/`](basics/README.md) | C 语言基础例程 18 个，从 `printf` 到链表、位运算、多文件编译。每个文件独立可跑，只讲一个主题 |
| [`soc_boot/`](soc_boot/README.md) | 从 0 到 `main` 的芯片启动流程 demo：向量表 → 搬 `.data` → 清 `.bss` → 时钟 → 串口 → 点亮 LED。含一份能烧进芯片的固件和一套主机模拟版 |
| [`pinctrl_demo/`](pinctrl_demo/README.md) | **专题**：pinctrl 子系统原理 —— pin → group → function，为什么引脚寄存器不能谁都写；带一个设备树版（改 `board.dts` 看效果）。单文件 demo，不需要内核也不需要硬件 |

> 注：内核驱动那一章（字符设备 / tty）的目录当前不在工作区，`make driver` / `make run-driver` 会自动跳过。

### 建议路线

1. `basics/01` ~ `04`：语法基础
2. `basics/05` `06`：指针（C 的分水岭）
3. `basics/12` `13` `14`：位运算、宏、函数指针 —— 看后两块之前最好先过一遍
4. `soc_boot/`：看这些语法在真实芯片上是怎么组合起来的（寄存器、向量表、MMIO）
5. `pinctrl_demo/`：把这些寄存器位域放到"多人共用、需要有人裁决"的场景里看

## 环境

gcc / arm-none-eabi-gcc（`soc_boot` 的真机版需要交叉编译器，没有也能跑主机模拟版）。
`pinctrl_demo` 只用 gcc，一条 `make run` 就能看完全过程。
