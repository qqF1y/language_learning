/*
 * 17_common_pitfalls.c —— C 语言常见坑合集
 *
 * 这里的原则是"只演示正确写法 + 用注释指出错误写法",
 * 因为真正的错误写法往往会崩溃或者产生不可预测的结果。
 *
 * 想亲眼看到出错过程, 可以把注释里的错误代码打开,
 * 然后用 make SAN=1 编译运行, 让 sanitizer 告诉你哪里错了。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ==================== 1. 数组越界 ==================== */
static void pit_array_bounds(void)
{
    puts("========== 1. 数组越界 ==========");

    int a[5] = {1, 2, 3, 4, 5};

    /* 正确: 用 sizeof 求长度, 循环条件写 < */
    printf("  合法访问: ");
    for (size_t i = 0; i < sizeof(a) / sizeof(a[0]); i++) {
        printf("%d ", a[i]);
    }
    putchar('\n');

    /* a[5] = 99;   错误! 下标范围是 0..4, 越界写会踩坏相邻的变量
     * 而且编译器不会报错, 运行时也可能"看起来正常" —— 最危险的一类 bug */
    puts("  a[5] = 99 是越界! C 不做边界检查, 这类 bug 极难排查。");
    puts("  排查: gcc -fsanitize=address 编译后运行, 会直接指出越界位置。");

    /* 常见错误: 循环写成 <= */
    puts("  最常见的错误写法: for (i = 0; i <= n; i++)  <-- 多跑一次就越界了");
}

/* ==================== 2. 字符串没有结尾符 ==================== */
static void pit_string_terminator(void)
{
    puts("\n========== 2. 字符串必须要有 '\\0' 结尾 ==========");

    /* 这不是字符串, 只是 5 个 char —— 没有 '\0' */
    char not_a_string[5] = {'h', 'e', 'l', 'l', 'o'};

    printf("  not_a_string 的内容(用 fwrite 按字节输出): ");
    fwrite(not_a_string, 1, sizeof(not_a_string), stdout);
    putchar('\n');
    puts("  如果对它用 printf(\"%s\", ...) 或 strlen(), 会一直往后读到随便一个 0 才停 ——");
    puts("  这就是经典的缓冲区越界读取, 可能打印出乱七八糟的东西甚至崩溃。");

    /* 正确写法: 留出 '\0' 的位置 */
    char good[] = "hello";              /* sizeof = 6 */
    printf("  正确写法 char good[] = \"hello\"; sizeof = %zu (含结尾的 '\\0')\n", sizeof(good));

    /* 手动构造时一定要自己补 */
    char manual[6];
    memcpy(manual, "hello", 5);
    manual[5] = '\0';
    printf("  手动构造后 strlen = %zu\n", strlen(manual));
}

/* ==================== 3. = 和 == ==================== */
static void pit_assign_vs_equal(void)
{
    puts("\n========== 3. 赋值 = 与比较 == ==========");

    int x = 0;

    /* 错误写法: if (x = 5) —— 这是赋值, 表达式值是 5, 永远为真!
     * 现代编译器会告警: "suggest parentheses around assignment used as truth value"
     * 但如果你写了双括号 if ((x = 5)) 就表示"我确实想赋值", 编译器就不告警了 */
    if (x == 5) {
        puts("  x == 5");
    } else {
        puts("  x != 5 (判断用的是 ==, 正确)");
    }

    /* 想把"赋值结果"当条件时, 用双括号明确表达意图 */
    if ((x = 5) != 0) {
        printf("  用 (x = 5) != 0 明确表示故意赋值, 现在 x = %d\n", x);
    }

    /* 经典防呆写法: 把常量写在左边, 写错了编译不过 */
    if (5 == x) {
        puts("  5 == x 这种\"常量在左\"的写法, 万一写成 5 = x 会直接编译报错");
    }
}

/* ==================== 4. sizeof 的误用 ==================== */
static int sum_wrong(const int *arr, size_t n)
{
    /* 在函数里 arr 只是一个指针, sizeof(arr) == 8 而不是数组总字节!
     * 所以长度必须作为参数传进来。 */
    int s = 0;
    for (size_t i = 0; i < n; i++) {
        s += arr[i];
    }
    return s;
}

static void pit_sizeof(void)
{
    puts("\n========== 4. sizeof 的坑 ==========");

    int a[10] = {0};

    printf("  main 里: sizeof(a) = %zu  (整个数组 40 字节)\n", sizeof(a));
    printf("  函数里: sizeof(参数) 只等于 %zu (指针大小)\n", sizeof(const int *));
    puts("  结论: sizeof 求数组长度只对\"真正的数组\"有效, 对指针无效。");

    int *p = a;
    printf("  sizeof(a)/sizeof(a[0]) = %zu  (正确)\n", sizeof(a) / sizeof(a[0]));

    size_t ptr_size  = sizeof(p);       /* 指针自身的大小: 8 字节 */
    size_t elem_size = sizeof(p[0]);    /* 元素的大小: 4 字节 */
    printf("  sizeof(p)/sizeof(p[0]) = %zu  (错误! 指针大小除以 int 大小)\n",
           ptr_size / elem_size);
    puts("  GCC 有个专门的警告 -Wsizeof-pointer-div 就是用来抓这个错误的。");

    printf("  sum_wrong(a, 10) = %d\n", sum_wrong(a, 10));

    /* 另一个坑: sizeof 的结果是无符号整数, 相减容易出问题 */
    printf("  sizeof(int) - 5 = %zu  <-- 无符号减法下溢变成巨大数!\n", sizeof(int) - 5);
    puts("  所以比较时建议写成: if (sizeof(x) > 5) 而不是 if (sizeof(x) - 5 > 0)");
}

/* ==================== 5. 有符号 / 无符号混用 ==================== */
static void pit_signed_unsigned(void)
{
    puts("\n========== 5. 有符号与无符号 ==========");

    int          i = -1;
    unsigned int u = 1;

    /* 不加转换的话 GCC 会给出 -Wsign-compare 告警, 效果其实和下面一样 */
    if ((unsigned int)i < u) {
        puts("  i < u 竟然成立?  (错误预期)");
    } else {
        printf("  i(%d) < u(%u) 是假的 —— 比较时 i 被转成了 %u\n", i, u, (unsigned int)i);
    }
    puts("  连编译器都看不下去这种比较, 所以它专门给了一个 -Wsign-compare 告警。");
    puts("  建议: 循环下标和长度统一用 size_t / int, 别混着比较。");

    /* 死循环经典案例 */
    puts("\n  经典死循环(这里只演示正确的写法, 不真的跑死循环):");
    puts("      for (unsigned int k = 10; k >= 0; k--) { }   // k 永远 >= 0, 死循环!");
    puts("      for (int k = 10; k >= 0; k--) { }            // 正确");
}

/* ==================== 6. 整数溢出 ==================== */
static void pit_overflow(void)
{
    puts("\n========== 6. 整数溢出 ==========");

    int a = 100000, b = 100000;

    /* a * b 是 int * int, 结果按 int 算, 已经溢出了(未定义行为) */
    puts("  错误: long long r = a * b;   // 先按 int 算完再转换, 已经溢出");
    printf("  正确: (long long)a * b = %lld\n", (long long)a * b);
    puts("  关键: 类型转换要发生在运算之前, 而不是之后。");

    /* 平均值也可能溢出 */
    int x = 2000000000, y = 2000000000;
    puts("\n  求平均值也不安全: (x + y) / 2 会溢出");
    printf("  正确写法 x + (y - x) / 2 = %d\n", x + (y - x) / 2);

    printf("  范围提醒: int 最大 %d, 大约 21 亿\n", INT_MAX);
    puts("  需要更大就用 long long(约 9.2e18) 或自己写大数。");
}

/* ==================== 7. 浮点数比较 ==================== */
static void pit_float(void)
{
    puts("\n========== 7. 浮点数比较 ==========");

    double a = 0.1 + 0.2;

    printf("  0.1 + 0.2 = %.20f\n", a);
    printf("  a == 0.3 ? %s\n", (a == 0.3) ? "真" : "假");
    puts("  浮点数在二进制里无法精确表示, 绝对不能用 == 比较!");

    /* 正确做法: 比较差值是否足够小 */
    double diff = a - 0.3;
    if (diff < 0) {
        diff = -diff;
    }
    if (diff < 1e-9) {
        printf("  用 fabs(a - 0.3) = %g < 1e-9 判断: 相等\n", diff);
    }

    puts("\n  其它注意点:");
    puts("    - 浮点数不要用来做精确的金额计算(用整数表示分, 或用 decimal 库)");
    puts("    - 累加大量小数时误差会累积");
    puts("    - printf 用 %.17g 才能完整还原一个 double");
}

/* ==================== 8. scanf / fgets 输入 ==================== */
static void pit_input(void)
{
    puts("\n========== 8. 输入处理的坑 ==========");

    char name[8] = {0};
    int  age = 0;

    printf("  请输入 \"姓名 年龄\" (可以故意把姓名输得很长试试): ");
    fflush(stdout);

    char buf[128];
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        puts("  (没有输入, 跳过)");
        return;
    }

    /* %7s 限制最多读 7 个字符到 name[8], 留一个位置给 '\0' */
    if (sscanf(buf, "%7s %d", name, &age) == 2) {
        printf("  姓名 = \"%s\", 年龄 = %d\n", name, age);
    } else {
        puts("  格式不对");
    }

    puts("  你输入的名字如果超过 7 个字符, 会被安全截断而不是冲垮内存。");
    puts("  对比: scanf(\"%s\", name) 不限制长度, 输入 100 个字符就直接栈溢出。");

    /* fgets 会保留换行符, 这是常见困扰 */
    char line[64];
    printf("  再随便输入一行, 演示去掉换行符: ");
    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) != NULL) {
        line[strcspn(line, "\n")] = '\0';       /* 经典做法: 找到 '\n' 换成 '\0' */
        printf("  去掉换行后: \"%s\" (长度 %zu)\n", line, strlen(line));
    } else {
        puts("  (没有输入, 跳过)");
    }

    puts("  另一个坑: scanf 遇到不匹配的输入会把它留在缓冲区里, 导致后面的读取立刻失败。");
    puts("  所以交互式输入通常 fgets + sscanf 组合使用。");
}

/* ==================== 9. getchar 的返回值 ==================== */
static void pit_getchar(void)
{
    puts("\n========== 9. getchar 必须用 int 接收 ==========");

    puts("  EOF 的值是 -1, 而 char 可能是无符号的, 存不下 -1。");
    puts("  错误写法: char c; while ((c = getchar()) != EOF)");
    puts("  正确写法: int  c; while ((c = getchar()) != EOF)");
    puts("  fgetc 同理。");
}

/* ==================== 10. 内存相关 ==================== */
static void pit_memory(void)
{
    puts("\n========== 10. 内存管理的坑 ==========");

    /* 10.1 忘记判空 */
    int *p = malloc(sizeof(int) * 1000000);
    if (p == NULL) {
        puts("  malloc 失败时返回 NULL, 必须在解引用之前判空");
        return;
    }
    p[0] = 1;
    free(p);
    p = NULL;                   /* 立刻置空 */

    /* 10.2 释放后继续用(只说明, 不真的执行) */
    puts("  free(p) 之后再用 p 叫 use-after-free, 是最常见的漏洞来源;");
    puts("  好习惯: free 完马上 p = NULL。");

    /* 10.3 free 两次 */
    puts("  重复 free 同一块内存会破坏堆结构, 崩溃点往往离出错点很远。");

    /* 10.4 内存泄漏 */
    puts("  忘记 free 就是内存泄漏。程序短暂运行看不出来, 长期运行会 OOM。");
    puts("  排查工具: valgrind --leak-check=full ./a.out");

    /* 10.5 返回局部变量的地址 */
    puts("  千万不要返回局部数组/局部变量的地址, 函数一返回它就不存在了:");
    puts("      char *bad(void) { char buf[16]; return buf; }   // 悬垂指针");

    /* 10.6 结构体浅拷贝 */
    puts("  含指针成员的结构体直接赋值, 只是拷贝了地址(浅拷贝),");
    puts("  两块结构体指向同一片堆内存, 释放时会 double free。需要深拷贝。");
}

/* ==================== 11. printf 格式不匹配 ==================== */
static void pit_printf(void)
{
    puts("\n========== 11. printf 格式符要和类型匹配 ==========");

    long long big = 1234567890123LL;
    size_t    sz  = sizeof(int);
    double    d   = 3.14;

    printf("  long long 用 %%lld: %lld\n", big);
    printf("  size_t 用 %%zu: %zu\n", sz);
    printf("  double 用 %%f: %.2f\n", d);

    puts("\n  写错类型(比如用 %d 打印 long long)不会报错, 而是打印出垃圾值,");
    puts("  因为 printf 只按你给的格式去解释栈上的字节。");
    puts("  编译时打开 -Wall 可以提前发现这类问题。");

    /* 一个典型: 忘了取地址 */
    int x = 5;
    printf("  打印变量要写变量本身: %d (如果写 &x 会得到地址)\n", x);
    puts("  反之 scanf 必须写 &x, 因为它需要地址才能写入。");
}

/* ==================== 12. 其它 ==================== */
static void pit_misc(void)
{
    puts("\n========== 12. 其它零碎但重要的点 ==========");
    puts("  - 局部变量不初始化: int a; printf(\"%d\", a);  // 内容随机");
    puts("  - 数组不能整体赋值: b = a; 错, 要用 memcpy 或逐个赋");
    puts("  - 结构体可以整体赋值: s1 = s2; 对");
    puts("  - 字符串不能用 == 比较: 那比的是地址, 要用 strcmp");
    puts("  - switch 的 case 后面忘了 break 会继续往下执行");
    puts("  - 头文件里的函数定义会被多次包含 => 多重定义错误, 要 .h 里只放声明");
    puts("  - 每条语句结束的 ; 忘了写, 报错往往出现在下一行");
    puts("  - 宏定义不加括号会出大事(见 13_preprocessor_macros.c)");
    puts("  - 在所有分支都有返回值, 否则函数返回随机值");
}

int main(void)
{
    pit_array_bounds();
    pit_string_terminator();
    pit_assign_vs_equal();
    pit_sizeof();
    pit_signed_unsigned();
    pit_overflow();
    pit_float();
    pit_input();
    pit_getchar();
    pit_memory();
    pit_printf();
    pit_misc();

    puts("\n============================================================");
    puts(" 想真正体会这些坑, 建议自己动手做两件事:");
    puts("   1. 把上面注释里的错误写法打开, 用 make SAN=1 跑一遍;");
    puts("   2. 每写一个数组/指针操作, 都问自己: 越界了吗? 初始化了吗? 释放了吗?");
    puts("============================================================");
    return 0;
}
