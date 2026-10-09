/*
 * stats.h —— 头文件: 只放“声明”和“类型定义”, 不放函数实现
 *
 * 头文件的三条规矩:
 *   1. 必须有 include guard(#ifndef / #define / #endif), 防止被重复包含
 *   2. 只放声明: 函数原型、结构体、宏、extern 变量
 *   3. 不要在这里定义变量或函数体, 否则多个 .c 一起链接时会报“重复定义”
 */
#ifndef STATS_H
#define STATS_H

#define STATS_NAME_MAX 32

/* 统计结果, 用结构体把多个结果打包返回 */
typedef struct {
    int    count;       /* 有效数据个数 */
    double sum;
    double min;
    double max;
    double mean;
} Stats;

/*
 * 计算一组数据的统计量。
 * 参数:
 *   data  数据首地址, 不能为 NULL
 *   n     数据个数, 必须 > 0
 *   out   输出参数, 用来回传结果
 * 返回: 1 表示成功, 0 表示失败
 */
int stats_compute(const double *data, int n, Stats *out);

/* 打印统计结果 */
void stats_print(const Stats *s);

#endif /* STATS_H */
