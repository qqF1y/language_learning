/*
 * 01_hello_io.c —— 第一个 C 程序：输出与输入
 *
 * 编译: gcc -std=c11 -Wall -Wextra -g 01_hello_io.c -o 01_hello_io
 * 运行: ./01_hello_io
 *
 * 知识点:
 *   1. 程序入口 main, 返回值 0 表示成功
 *   2. printf 的常用格式占位符
 *   3. 宽度 / 对齐 / 补零
 *   4. scanf 读取输入(以及为什么生产代码更推荐 fgets + sscanf)
 */
#include <stdio.h>

int main(void)
{
    /* ---------- 1. 最简单的输出 ---------- */
    printf("Hello, World!\n");          /* \n 是换行, C 里必须自己写 */
    puts("puts 会自动加换行, 但只能输出字符串");

    /* ---------- 2. 常用格式占位符 ---------- */
    char               c  = 'A';
    int                i  = -42;
    unsigned int       u  = 42u;
    long               l  = 1234567890L;
    long long          ll = 1234567890123456789LL;
    float              f  = 3.14159f;
    double             d  = 3.141592653589793;
    const char        *s  = "hello";

    printf("\n---------- 格式化输出 ----------\n");
    printf("char      %%c -> %c      (也可以用 %%d 打印它的编码: %d)\n", c, c);
    printf("int       %%d -> %d\n", i);
    printf("unsigned  %%u -> %u\n", u);
    printf("long      %%ld -> %ld\n", l);
    printf("long long %%lld -> %lld\n", ll);
    printf("float     %%f -> %.2f   (传入时会被提升为 double)\n", (double)f);
    printf("double    %%f -> %.12f\n", d);
    printf("string    %%s -> %s\n", s);
    printf("pointer   %%p -> %p\n", (void *)s);

    puts("\n-- %% 是转义写法: 想打印一个百分号要写 %%，因为 % 是格式引导符 --");
    printf("完成度: %d%%\n", 80);

    /* ---------- 3. 宽度、对齐、补零 ---------- */
    puts("\n---------- 宽度控制 ----------");
    printf("|%5d|%-5d|%05d|\n", 42, 42, 42);       /* 右对齐 / 左对齐 / 补零 */
    printf("|%10.3f|%-10.3f|\n", d, d);            /* 总宽 10, 小数点后 3 位 */
    printf("十六进制: %#x  八进制: %#o  大写十六进制: %#X\n", 255, 255, 255);
    printf("科学计数法: %e\n", 0.000123);

    /* ---------- 4. 读取输入 ---------- */
    puts("\n---------- 输入 ----------");
    int  age = 0;
    char name[64] = {0};

    printf("请输入姓名和年龄(例如 Tom 18): ");
    fflush(stdout);                                 /* 确保提示先打印出来 */

    /* %63s 限制最多读 63 个字符 + '\0', 防止溢出 name[64] */
    if (scanf("%63s %d", name, &age) == 2) {
        printf("你好 %s, 明年你 %d 岁\n", name, age + 1);
    } else {
        puts("(没读到合法输入, 直接跳过 —— 用 < /dev/null 运行时就走到这里)");
    }

    /*
     * 提醒: scanf("%s", buf) 不检查长度, 输入过长会直接冲垮栈(缓冲区溢出)。
     * 工业代码一般这样写:
     *
     *   char line[128];
     *   if (fgets(line, sizeof(line), stdin) != NULL) {
     *       sscanf(line, "%63s %d", name, &age);
     *   }
     */
    return 0;
}
