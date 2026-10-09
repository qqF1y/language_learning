/*
 * 02_types_operators.c —— 数据类型、运算符与类型转换
 *
 * C 的类型系统是"信任程序员"的: 编译器几乎不做检查, 错了也不一定报错,
 * 而是产生一个看起来正常但完全错误的结果。这个文件把常见的坑都点一遍。
 */
#include <stdio.h>
#include <limits.h>     /* INT_MAX, UINT_MAX ... */
#include <float.h>      /* DBL_MAX ... */
#include <stdint.h>     /* int32_t, uint64_t ... 固定宽度类型 */
#include <stddef.h>     /* size_t */

int main(void)
{
    /* ==================== 1. sizeof: 各类型占多少字节 ==================== */
    puts("========== 1. sizeof ==========");
    printf("sizeof(char)      = %zu 字节  (C 标准规定就是 1)\n", sizeof(char));
    printf("sizeof(short)     = %zu 字节\n", sizeof(short));
    printf("sizeof(int)       = %zu 字节  (通常 4, 但标准没规定死)\n", sizeof(int));
    printf("sizeof(long)      = %zu 字节\n", sizeof(long));
    printf("sizeof(long long) = %zu 字节\n", sizeof(long long));
    printf("sizeof(float)     = %zu 字节\n", sizeof(float));
    printf("sizeof(double)    = %zu 字节\n", sizeof(double));
    printf("sizeof(void *)    = %zu 字节\n", sizeof(void *));
    printf("sizeof(size_t)    = %zu 字节  (用它表示长度/下标最合适)\n", sizeof(size_t));

    puts("\n-- 跨平台要精确控制位数, 用 stdint.h 的固定宽度类型 --");
    printf("int8_t=%zu uint8_t=%zu int16_t=%zu int32_t=%zu int64_t=%zu\n",
           sizeof(int8_t), sizeof(uint8_t), sizeof(int16_t),
           sizeof(int32_t), sizeof(int64_t));

    /* ==================== 2. 取值范围 ==================== */
    puts("\n========== 2. 取值范围 ==========");
    printf("CHAR_MAX  = %d, CHAR_MIN  = %d\n", CHAR_MAX, CHAR_MIN);
    printf("INT_MAX   = %d, INT_MIN   = %d\n", INT_MAX, INT_MIN);
    printf("UINT_MAX  = %u\n", UINT_MAX);
    printf("LLONG_MAX = %lld\n", LLONG_MAX);
    printf("DBL_MAX   = %e, DBL_MIN = %e\n", DBL_MAX, DBL_MIN);

    /* ==================== 3. 整数除法 vs 浮点除法 ==================== */
    puts("\n========== 3. 除法 ==========");
    int a = 7, b = 2;
    printf("7 / 2       = %d     <-- 两个整数相除, 结果还是整数, 小数被丢掉\n", a / b);
    printf("7 %% 2       = %d     <-- %% 是取余数\n", a % b);
    printf("7 / 2.0     = %.2f  <-- 只要有一个是浮点数, 就按浮点算\n", a / 2.0);
    printf("(double)a/b = %.2f  <-- 推荐写法: 显式转换, 意图明确\n", (double)a / b);
    printf("-7 / 2      = %d, -7 %% 2 = %d  <-- C99 起: 除法向 0 取整, 余数符号跟被除数\n",
           -7 / 2, -7 % 2);

    /* ==================== 4. 溢出 ==================== */
    puts("\n========== 4. 溢出 ==========");
    unsigned int ubig = UINT_MAX;
    printf("UINT_MAX     = %u\n", ubig);
    printf("UINT_MAX + 1 = %u   <-- 无符号溢出会“回绕”, 行为是确定的\n", ubig + 1u);

    puts("有符号溢出(如 INT_MAX + 1) 是 未定义行为(UB), 编译器可以做任何事:");
    puts("  它可能得到负数、可能被优化掉、可能崩溃 —— 永远不要依赖它。");
    puts("正确做法是先判断: if (x > INT_MAX - y) { 处理溢出 }");

    /* ==================== 5. 隐式类型转换的坑 ==================== */
    puts("\n========== 5. 隐式转换 ==========");
    unsigned int u = 1;
    int          i = -1;
    /* 这里写 (unsigned int)i 是"显式"转换; 不写的话编译器会自动转(并给出 -Wsign-compare 告警),
     * 效果完全一样: -1 变成 4294967295, 于是 i < u 变成假 */
    if ((unsigned int)i < u) {
        puts("i < u 成立  (理论上的错误结果)");
    } else {
        printf("i(%d) < u(%u) 为假!  <-- 比较时 i 被转成无符号, -1 变成了 %u\n",
               i, u, (unsigned int)i);
    }

    printf("(int)3.9  = %d, (int)-3.9 = %d   <-- 强制转换是“截断”, 不是四舍五入\n",
           (int)3.9, (int)-3.9);
    printf("四舍五入要自己写: (int)(3.9 + 0.5) = %d\n", (int)(3.9 + 0.5));

    char ch = 'A';
    printf("'A' + 1 = %d, 当成字符打印 = '%c'   <-- char 参与运算时先提升为 int\n",
           ch + 1, ch + 1);

    /* ==================== 6. 浮点数 ==================== */
    puts("\n========== 6. 浮点数 ==========");
    double sum = 0.1 + 0.2;
    printf("0.1 + 0.2 = %.20f\n", sum);
    printf("0.1 + 0.2 == 0.3 ? %s   <-- 二进制无法精确表示 0.1, 浮点数不能用 == 比较\n",
           (sum == 0.3) ? "真" : "假");
    puts("正确做法: fabs(a - b) < 1e-9  (需要 #include <math.h>, 链接加 -lm)");

    /* ==================== 7. 自增自减与复合赋值 ==================== */
    puts("\n========== 7. 自增/复合赋值 ==========");
    int x = 5;
    int post = x++;                 /* 后缀: 先用旧值, 再加 */
    printf("x=5, post = x++ 得到 %d, 之后 x = %d\n", post, x);

    x = 5;
    int pre = ++x;                  /* 前缀: 先加, 再用新值 */
    printf("x=5, pre  = ++x 得到 %d, 之后 x = %d\n", pre, x);

    puts("注意: printf(\"%d %d\", x++, x++) 这种写法求值顺序未定义, 不要写!");

    x = 10;
    x += 5;  printf("x += 5  -> %d\n", x);
    x *= 2;  printf("x *= 2  -> %d\n", x);
    x -= 3;  printf("x -= 3  -> %d\n", x);
    x >>= 1; printf("x >>= 1 -> %d  (右移一位相当于除以 2)\n", x);

    /* ==================== 8. 逻辑运算与短路 ==================== */
    puts("\n========== 8. 三目运算与短路求值 ==========");
    int score = 76;
    printf("score=%d -> %s\n", score, score >= 60 ? "及格" : "不及格");

    int n = 0;
    /* && 短路: 左边为假时右边根本不执行, 所以 100/n 不会执行到, 不会除零 */
    if (n != 0 && 100 / n > 5) {
        puts("不会走到这里");
    } else {
        puts("n == 0, 多亏 && 的短路特性避免了除零崩溃");
    }

    /* || 短路: 左边为真时右边不执行 */
    if (n == 0 || 100 / n > 5) {
        puts("n == 0 已经为真, 右边同样没执行");
    }

    /* 写 A && B 时, 请把“便宜、能挡错”的条件放左边, 这是常见写法:
     *   if (p != NULL && p->value > 0) ...        // 先判空再用
     *   if (idx >= 0 && idx < len && a[idx] ...)  // 先判范围再访问
     */
    return 0;
}
