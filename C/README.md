# C 语言经典例程

一套从零开始的 C 语言学习例程。每个文件都是**独立可编译运行**的小程序，
只讲一个主题，注释里写清楚"为什么这么做"和"常见的坑"。

## 快速开始

```bash
cd C

make                    # 编译全部到 build/
make run                # 编译并依次运行全部例程
make one FILE=05_pointers_basics    # 只跑某一个
make clean              # 清理

# 不用 make 也行:
gcc -std=c11 -Wall -Wextra -g 05_pointers_basics.c -o demo && ./demo
```

编译选项说明：

| 选项 | 作用 |
|------|------|
| `-std=c11` | 用 C11 标准 |
| `-Wall -Wextra -Wpedantic` | 打开绝大多数警告，**警告基本等于 bug** |
| `-g -O0` | 带调试信息，方便 gdb |
| `-fsanitize=address,undefined` | `make SAN=1 run`，抓越界/泄漏/未定义行为 |

## 例程目录

| 文件 | 主题 |
|------|------|
| `01_hello_io.c` | Hello World、`printf` 格式占位符、宽度对齐、`scanf` 输入 |
| `02_types_operators.c` | 数据类型、`sizeof`、取值范围、运算符、类型转换与溢出 |
| `03_control_flow.c` | `if/else`、`switch`（贯穿）、`while/do-while`、`break/continue`、`goto` 清理 |
| `04_arrays_strings.c` | 一维/二维数组、字符串与 `'\0'`、`strlen/strcmp/strstr/strtok` |
| `05_pointers_basics.c` | 地址与解引用、值传递 vs 指针传递、指针步长、`const` 三种写法、多级指针 |
| `06_pointers_and_array.c` | 数组退化、`arr[i]` 与 `*(arr+i)`、指针数组 vs 数组指针、行指针、`memcpy` |
| `07_functions_recursion.c` | 函数原型、`static` 局部变量、作用域、递归（阶乘/斐波那契/汉诺塔/反转） |
| `08_struct_union_enum.c` | 结构体、嵌套、指针访问 `->`、`enum`、`union`、位域、内存布局 |
| `09_dynamic_memory.c` | `malloc/calloc/realloc/free`、动态数组扩容、动态二维数组、`strdup` |
| `10_linked_list.c` | 单链表：头插/尾插/查找/删除/原地反转/释放 |
| `11_file_io.c` | 文本文件读写、追加、`fgets`、字数统计、二进制 `fwrite/fread`、`fseek/ftell` |
| `12_bit_operations.c` | 位运算、置位/清位/取位、`popcount`、位图、RGB 拆包、hex dump |
| `13_preprocessor_macros.c` | 宏括号陷阱、`#`/`##`、条件编译、日志宏、`X-Macro` |
| `14_function_pointers.c` | 函数指针、回调、分发表、`qsort` 比较函数 |
| `15_sort_search.c` | 冒泡/选择/插入/快排、`qsort`、线性/二分查找、性能对比 |
| `16_classic_puzzles.c` | 九九表、素数、水仙花、回文、进制转换、杨辉三角、质因数分解、完数 |
| `17_common_pitfalls.c` | 常见坑合集：越界、未终止字符串、`=` vs `==`、有符号/无符号、浮点比较 |
| `18_multi_file/` | 多文件编译：头文件守卫、声明与定义分离、`static` 函数、分开编译再链接 |

## 建议学习顺序

1. 语法基础：`01` → `02` → `03` → `04`
2. 核心难点：`05` → `06`（指针是 C 的分水岭，多花时间）
3. 组织代码：`07` → `13` → `18`
4. 数据结构与内存：`08` → `09` → `10`
5. 实用技能：`11` → `12` → `14` → `15`
6. 刷题与避坑：`16` → `17`

## 调试工具

```bash
gcc -g -fsanitize=address,undefined 09_dynamic_memory.c -o demo   # 内存错误
valgrind --leak-check=full ./demo                                  # 内存泄漏
gdb ./demo                                                         # 单步调试
gcc -E 13_preprocessor_macros.c | less                             # 看宏展开结果
```

## 18_multi_file 的编译过程

```bash
cd 18_multi_file
gcc -c stats.c          # 编译成目标文件 stats.o
gcc -c main.c           # 编译成 main.o
gcc main.o stats.o -o app   # 链接
./app
```

要点：头文件里只放**声明**，`.c` 里放**定义**；头文件必须带 include guard。
