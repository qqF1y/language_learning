/*
 * 03_control_flow.c —— 分支与循环
 *
 * 技巧: 一个知识点写成一个 static 函数, main 里按顺序调用。
 *       这样既方便单独阅读, 也方便临时注释掉某一段。
 */
#include <stdio.h>
#include <stdlib.h>     /* malloc / free */

/* ==================== if / else ==================== */
static void demo_if_else(void)
{
    puts("========== 1. if / else if / else ==========");

    int score = 87;

    if (score >= 90) {
        printf("score=%d -> 优秀\n", score);
    } else if (score >= 80) {
        printf("score=%d -> 良好\n", score);
    } else if (score >= 60) {
        printf("score=%d -> 及格\n", score);
    } else {
        printf("score=%d -> 不及格\n", score);
    }

    /* 简单二选一时, 三目运算符更紧凑 */
    const char *pass = (score >= 60) ? "通过" : "未通过";
    printf("三目运算: %s\n", pass);

    /* 常见坑: if (a = 5) 是赋值, 永远为真! 编译器会告警, 请写 == */
}

/* ==================== switch ==================== */
static void demo_switch(void)
{
    puts("\n========== 2. switch ==========");

    /* 2.1 基本用法: 每个 case 后面通常要 break */
    int cmd = 2;
    switch (cmd) {
    case 1:
        puts("cmd=1: 打开文件");
        break;
    case 2:
        puts("cmd=2: 保存文件");
        break;
    case 3:
        puts("cmd=3: 关闭文件");
        break;
    default:
        printf("cmd=%d: 未知命令\n", cmd);
        break;
    }

    /* 2.2 故意“贯穿”(fallthrough): 多个 case 共用一段逻辑 */
    for (int m = 1; m <= 13; m += 12) {
        switch (m) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12:
            printf("%2d 月有 31 天\n", m);
            break;
        case 4: case 6: case 9: case 11:
            printf("%2d 月有 30 天\n", m);
            break;
        case 2:
            puts(" 2 月有 28/29 天");
            break;
        default:
            printf("%2d 不是合法月份\n", m);
            break;
        }
    }
    puts("(C 里贯穿不会报错, 忘了 break 是最常见的 bug 之一)");

    /* 2.3 switch 里声明变量要加花括号限定作用域 */
    int type = 1;
    switch (type) {
    case 0: {
        int local = 100;
        printf("case 0, local = %d\n", local);
        break;
    }
    case 1:
        puts("case 1, 简单分支");
        break;
    default:
        break;
    }
}

/* ==================== 循环 ==================== */
static void demo_loops(void)
{
    puts("\n========== 3. 三种循环 ==========");

    /* for: 循环次数已知 */
    int sum = 0;
    for (int i = 1; i <= 10; i++) {
        sum += i;
    }
    printf("for:  1+2+...+10 = %d\n", sum);

    /* while: 条件驱动, 次数未知 */
    int n = 12345, rev = 0;
    while (n > 0) {
        rev = rev * 10 + n % 10;    /* 每次取出最低位拼到结果末尾 */
        n /= 10;
    }
    printf("while: 12345 反转 = %d\n", rev);

    /* do-while: 至少执行一次, 常用于“先做再判断” */
    int x = 100;
    do {
        printf("do-while: 循环体执行了, x = %d\n", x);
        x++;
    } while (x < 3);                /* 条件一开始就为假, 但循环体已经跑过一遍 */

    while (x < 3) {
        puts("while: 这段永远不会执行");
    }

    /* 遍历数组的惯用写法: 下标用 size_t, 和 sizeof 的结果类型一致 */
    int a[] = {3, 1, 4, 1, 5};
    size_t len = sizeof(a) / sizeof(a[0]);      /* 求数组元素个数 */
    printf("数组长度 = %zu, 元素: ", len);
    for (size_t i = 0; i < len; i++) {
        printf("%d ", a[i]);
    }
    putchar('\n');

    /* 死循环的正确写法 */
    int guard = 0;
    for (;;) {
        if (++guard > 3) {
            break;
        }
    }
    printf("for(;;) + break 退出了, guard = %d\n", guard);
}

/* ==================== break / continue ==================== */
static void demo_break_continue(void)
{
    puts("\n========== 4. break 与 continue ==========");

    /* break: 提前结束整个循环 */
    for (int i = 1; i <= 100; i++) {
        if (i % 7 == 0) {
            printf("break:    1..100 中第一个能被 7 整除的数是 %d\n", i);
            break;
        }
    }

    /* continue: 跳过本次剩余语句, 直接进入下一次 */
    printf("continue: 1..20 中的奇数: ");
    for (int i = 1; i <= 20; i++) {
        if (i % 2 == 0) {
            continue;
        }
        printf("%d ", i);
    }
    putchar('\n');

    /* 嵌套循环里 break 只跳出最近的一层 */
    puts("嵌套 break 只跳一层, 输出 i=0 j=0 / i=1 j=0 / i=2 j=0:");
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (j == 1) {
                break;          /* 只跳出内层 */
            }
            printf("  i=%d j=%d\n", i, j);
        }
    }

    /* 想一次跳出多层循环: 用标志位, 或者 goto */
}

/* ==================== goto 的正当用途 ==================== */
static int demo_goto(void)
{
    puts("\n========== 5. goto 的正确用法: 统一错误处理 ==========");

    int ret = 0;
    char *res1 = NULL;
    char *res2 = NULL;

    res1 = malloc(16);
    if (res1 == NULL) {
        ret = 1;
        goto out;               /* 申请失败, 跳到统一的清理入口 */
    }

    res2 = malloc(16);
    if (res2 == NULL) {
        ret = 2;
        goto out;
    }

    printf("两个资源都申请成功: %p, %p\n", (void *)res1, (void *)res2);
    /* 这里省略真实业务, 假设中途出错 */
    ret = 0;

out:
    free(res1);                 /* free(NULL) 是安全的, 所以不需要额外判断 */
    free(res2);
    printf("goto out: 资源已统一释放, ret = %d\n", ret);
    return ret;
}

/* ==================== 嵌套循环打印图形 ==================== */
static void demo_pattern(void)
{
    puts("\n========== 6. 嵌套循环打印图形 ==========");

    const int rows = 5;

    for (int i = 1; i <= rows; i++) {
        for (int j = 0; j < i; j++) {
            putchar('*');
        }
        putchar('\n');
    }
    putchar('\n');

    for (int i = rows; i >= 1; i--) {
        for (int j = 0; j < rows - i; j++) {
            putchar(' ');                   /* 前面的空格 */
        }
        for (int j = 0; j < 2 * i - 1; j++) {
            putchar('*');                   /* 中间的星号 */
        }
        putchar('\n');
    }
}

int main(void)
{
    demo_if_else();
    demo_switch();
    demo_loops();
    demo_break_continue();
    demo_goto();
    demo_pattern();
    return 0;
}
