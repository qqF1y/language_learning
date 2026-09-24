/*
 * stats.c —— 实现文件: 放真正的函数定义
 *
 * 这个文件 #include 自己的头文件, 好处是:
 * 编译器会帮你检查"定义和声明是否一致", 不一致会直接报错。
 */
#include "stats.h"

#include <stdio.h>

/* static 函数: 只在本文件可见, 别的 .c 文件链接不到它。
 * 作用是把实现细节藏起来, 避免命名冲突 —— 相当于 C 里的"私有函数"。 */
static int stats_is_finite(double x)
{
    /* 简单的 NaN / Inf 检查: NaN 不等于自己, Inf 减去自己还是 Inf */
    return (x == x) && (x - x == 0.0);
}

int stats_compute(const double *data, int n, Stats *out)
{
    if (data == NULL || out == NULL || n <= 0) {
        return 0;                       /* 参数检查, 永远不要跳过 */
    }

    out->count = 0;
    out->sum   = 0.0;
    out->min   = 0.0;
    out->max   = 0.0;
    out->mean  = 0.0;

    for (int i = 0; i < n; i++) {
        if (!stats_is_finite(data[i])) {
            continue;                   /* 跳过 NaN / Inf */
        }

        if (out->count == 0) {          /* 第一个有效数据同时初始化 min/max */
            out->min = data[i];
            out->max = data[i];
        } else {
            if (data[i] < out->min) {
                out->min = data[i];
            }
            if (data[i] > out->max) {
                out->max = data[i];
            }
        }

        out->sum += data[i];
        out->count++;
    }

    if (out->count == 0) {
        return 0;                       /* 一条有效数据都没有 */
    }

    out->mean = out->sum / out->count;
    return 1;
}

void stats_print(const Stats *s)
{
    if (s == NULL) {
        return;
    }

    printf("有效数据: %d 个\n", s->count);
    printf("总和    : %.2f\n", s->sum);
    printf("最小值  : %.2f\n", s->min);
    printf("最大值  : %.2f\n", s->max);
    printf("平均值  : %.4f\n", s->mean);
}
