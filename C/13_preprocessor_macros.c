/*
 * 13_preprocessor_macros.c —— 预处理器与宏
 *
 * 预处理器是"编译之前的一遍纯文本替换"。它不懂类型、不懂作用域,
 * 只认 # 开头的命令。威力大, 坑也大。
 *
 * 想看看真实展开结果, 运行:
 *     gcc -E 13_preprocessor_macros.c | less
 */
#include <stdio.h>
#include <stdlib.h>

/* ==================== 1. 常量宏 ==================== */
#define PI             3.14159265358979
#define MAX_BUF        256
#define VERSION_STRING "1.0.0"

/* 宏属于全局, 从定义处一直有效到 #undef 或文件结束, 没有作用域概念 */

/* ==================== 2. 函数式宏: 括号是生命线 ==================== */
#define SQUARE_BAD(x)   (x * x)                 /* 参数没加括号 -> 有坑 */
#define SQUARE(x)       ((x) * (x))             /* 正确: 参数和整体都加括号 */
#define MAX2(a, b)      ((a) > (b) ? (a) : (b))
#define MIN2(a, b)      ((a) < (b) ? (a) : (b))
#define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))

/* ==================== 3. # 字符串化 与 ## 连接 ==================== */
#define STR(x)          #x                      /* 把参数变成字符串 */
#define XSTR(x)         STR(x)                  /* 中转一层, 让参数先展开 */
#define CONCAT(a, b)    a##b                    /* 把两个记号拼成一个记号 */

/* ==================== 4. 日志与断言宏 ==================== */
/* __FILE__ / __LINE__ 是预处理器提供的, 能定位到出错的具体位置 */
#define LOG(fmt, ...) \
    fprintf(stderr, "[LOG %s:%d] " fmt "\n", __FILE__, __LINE__, __VA_ARGS__)

/* do { ... } while (0) 是宏里包多语句的标准写法:
 * 这样在 if (...) MACRO(); 后面加分号也完全正确 */
#define ASSERT(cond)                                                          \
    do {                                                                      \
        if (!(cond)) {                                                        \
            fprintf(stderr, "断言失败: %s   (%s:%d)\n",                        \
                    #cond, __FILE__, __LINE__);                               \
            abort();                                                          \
        }                                                                     \
    } while (0)

/* ==================== 5. 条件编译 ==================== */
/* 改这个数字试试 0 / 1 / 2, 看看哪些代码被编译进去 */
#define DEBUG_LEVEL 2

#if DEBUG_LEVEL >= 2
#define DBG(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", __VA_ARGS__)
#elif DEBUG_LEVEL == 1
#define DBG(fmt, ...) fprintf(stderr, "[INFO ] " fmt "\n", __VA_ARGS__)
#else
#define DBG(fmt, ...) ((void)0)                 /* 一行都不用编译进去 */
#endif

/* ==================== 6. X-Macro: 一份数据生成多份代码 ==================== */
/* 加一个颜色只需要在这里加一行, 枚举和名字表会自动同步 */
#define COLOR_LIST(X) \
    X(RED)            \
    X(GREEN)          \
    X(BLUE)

typedef enum {
#define X(name) COLOR_##name,
    COLOR_LIST(X)
#undef X
    COLOR_COUNT
} Color;

static const char *const g_color_names[] = {
#define X(name) #name,
    COLOR_LIST(X)
#undef X
};

int main(void)
{
    /* ==================== 1. 常量宏 ==================== */
    puts("========== 1. 常量宏 ==========");
    printf("  PI = %.6f\n", PI);
    printf("  MAX_BUF = %d, VERSION = %s\n", MAX_BUF, VERSION_STRING);
    puts("  现代写法推荐: 带类型的用 const 变量, 需要编译期常量的地方才用 #define");
    puts("      const double pi = 3.14159265358979;   // 有类型, 可调试");

    /* ==================== 2. 括号陷阱 ==================== */
    puts("\n========== 2. 宏的括号陷阱 ==========");

    printf("  SQUARE_BAD(2 + 3) = %d\n", SQUARE_BAD(2 + 3));
    puts("      展开后是 (2 + 3 * 2 + 3) = 11, 完全错了!");
    printf("  SQUARE(2 + 3)     = %d\n", SQUARE(2 + 3));
    puts("      展开后是 ((2 + 3) * (2 + 3)) = 25, 正确");

    printf("  MAX2(3, 5) = %d, MIN2(3, 5) = %d\n", MAX2(3, 5), MIN2(3, 5));

    int arr[] = {1, 2, 3, 4, 5};
    printf("  ARRAY_SIZE(arr) = %zu  (对真正的数组有效)\n", ARRAY_SIZE(arr));
    puts("      但如果 arr 是函数参数(已退化成指针), 结果就是指针大小, 会出错");

    puts("\n  宏的第二个坑: 参数被求值多次");
    puts("      MAX2(i++, j++) 会让 i 和 j 各自自增两次!");
    puts("      有副作用或逻辑复杂时, 请写成 static inline 函数:");
    puts("          static inline int max2(int a, int b) { return a > b ? a : b; }");

    /* ==================== 3. # 与 ## ==================== */
    puts("\n========== 3. # 字符串化 与 ## 连接 ==========");

    int a1 = 10, a2 = 20, a3 = 30;
    printf("  CONCAT(a, 1) = %d, CONCAT(a, 2) = %d, CONCAT(a, 3) = %d\n",
           CONCAT(a, 1), CONCAT(a, 2), CONCAT(a, 3));

    printf("  STR(3 + 4)  = \"%s\"\n", STR(3 + 4));
    printf("  STR(PI)     = \"%s\"\n", STR(PI));
    printf("  XSTR(PI)    = \"%s\"  <-- 两层宏才会先把 PI 展开成数值\n", XSTR(PI));
    puts("      这个 \"加一层中转\" 的技巧在生成变量名时非常常用。");

    /* ==================== 4. 日志与断言 ==================== */
    puts("\n========== 4. 日志宏(输出在 stderr) ==========");

    int x = 42;
    const char *who = "world";
    LOG("x=%d, who=%s", x, who);
    LOG("也可以只传一个参数: x=%d", x);
    puts("  (宏里的 ... 和 __VA_ARGS__ 表示\"可变参数\", 至少要传一个, 否则是 C99 之外的用法)");

    ASSERT(x == 42);
    puts("  ASSERT(x == 42) 通过, 程序继续");
    ASSERT(ARRAY_SIZE(arr) == 5);
    puts("  ASSERT(ARRAY_SIZE(arr) == 5) 通过");
    /* ASSERT(x == 0);   打开这一行: 会打印文件名/行号/条件并 abort() */

    /* ==================== 5. 条件编译 ==================== */
    puts("\n========== 5. 条件编译 ==========");
    printf("  当前 DEBUG_LEVEL = %d\n", DEBUG_LEVEL);

    DBG("这条日志只在 DEBUG_LEVEL >= 1 时才会被编译进去, 当前 level=%d", DEBUG_LEVEL);

#if DEBUG_LEVEL >= 2
    puts("  DEBUG_LEVEL >= 2: 这个代码块被编译了");
#else
    puts("  DEBUG_LEVEL < 2: 走到 else 分支");
#endif

    /* 常见用途: 平台差异 */
#if defined(__linux__)
    puts("  当前平台: Linux (__linux__ 已定义)");
#elif defined(_WIN32)
    puts("  当前平台: Windows");
#else
    puts("  当前平台: 未知");
#endif

    /* 头文件守卫也是条件编译, 见 18_multi_file/stats.h */

    /* ==================== 6. X-Macro ==================== */
    puts("\n========== 6. X-Macro: 一份数据生成枚举 + 名字表 ==========");
    printf("  COLOR_RED=%d COLOR_GREEN=%d COLOR_BLUE=%d COLOR_COUNT=%d\n",
           COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_COUNT);

    for (int c = 0; c < COLOR_COUNT; c++) {
        printf("  颜色[%d] = %s\n", c, g_color_names[c]);
    }
    puts("  枚举和字符串表永远同步, 不会出现\"加了枚举忘了加名字\"的 bug。");

    /* ==================== 7. 预定义宏 ==================== */
    puts("\n========== 7. 预处理器预定义的宏 ==========");
    printf("  __FILE__         = %s\n", __FILE__);
    printf("  __LINE__         = %d  (这一行所在的行号)\n", __LINE__);
    printf("  __DATE__         = %s\n", __DATE__);
    printf("  __TIME__         = %s\n", __TIME__);
    printf("  __STDC_VERSION__ = %ld\n", (long)__STDC_VERSION__);
    printf("  __func__         = %s  (这个是编译器提供的, 不是宏)\n", __func__);

    puts("\n  注意: __DATE__/__TIME__ 在编译时确定, 想让它们更新必须重新编译。");

    puts("\n========== 8. 用宏要守的规矩 ==========");
    puts("  1. 参数和整体都加括号");
    puts("  2. 一行放不下用 \\ 续行, 反斜杠后面不能有空格");
    puts("  3. 多语句用 do { ... } while (0) 包起来");
    puts("  4. 别用宏做有副作用的事(自增、赋值、函数调用)");
    puts("  5. 能用 const / enum / static inline 函数替代就别用宏");
    puts("  6. 宏名全大写, 提示读者这是宏");
    return 0;
}
