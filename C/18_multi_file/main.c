/*
 * main.c —— 多文件编译示例的主程序
 *
 * 编译方式一(一步到位):
 *     gcc -std=c11 -Wall -Wextra main.c stats.c -o app
 *
 * 编译方式二(分开编译再链接, 这才是大项目的做法):
 *     gcc -c stats.c -o stats.o     # 编译: 每个 .c 独立编译成目标文件
 *     gcc -c main.c  -o main.o
 *     gcc main.o stats.o -o app     # 链接: 把目标文件拼到一起
 *     ./app
 *
 * 分开编译的好处: 改一个文件只需要重编它, 不用把整个项目重编一遍。
 * make 干的就是这件事。
 */
#include <stdio.h>

#include "stats.h"      /* 用 "" 而不是 <> : 先在本目录找, 适合自己的头文件 */

int main(void)
{
    double scores[] = { 88.5, 92.0, 79.5, 95.5, 60.0, 73.5, 85.0, 100.0 };
    int    n = (int)(sizeof(scores) / sizeof(scores[0]));

    printf("输入数据(%d 个): ", n);
    for (int i = 0; i < n; i++) {
        printf("%.1f ", scores[i]);
    }
    putchar('\n');
    putchar('\n');

    Stats st;                       /* Stats 类型来自 stats.h */
    if (!stats_compute(scores, n, &st)) {
        puts("统计失败!");
        return 1;
    }
    stats_print(&st);               /* 实现在 stats.c 里 */

    /* --- 再试一组包含异常值的数据 --- */
    puts("\n---------- 边界测试 ----------");
    puts("(把成绩换成 0 个 / NULL, 看看参数检查是否生效)");

    if (!stats_compute(NULL, n, &st)) {
        puts("传入 NULL 被正确拒绝");
    }
    if (!stats_compute(scores, 0, &st)) {
        puts("传入 n=0 被正确拒绝");
    }

    puts("\n---------- 这样组织代码的好处 ----------");
    puts("  main.c       只关心\"怎么用\", 不关心内部怎么算");
    puts("  stats.h      对外接口(合同): 谁都能看, 一改所有人都知道");
    puts("  stats.c      内部实现: 可以随便重写, 只要接口不变就不影响别人");
    puts("  static 函数  内部工具函数, 外界看不到, 也不会撞名字");

    puts("\n---------- 常见错误 ----------");
    puts("  1. 头文件里写函数定义  -> 多个 .c 包含它时报 multiple definition");
    puts("  2. 忘了 include guard -> 同一文件里重复包含时报 redefinition");
    puts("  3. 声明和定义不一致    -> 编译能过, 链接时报 undefined reference");
    puts("  4. 忘了把某个 .o 加进链接命令 -> undefined reference to 'xxx'");
    return 0;
}
