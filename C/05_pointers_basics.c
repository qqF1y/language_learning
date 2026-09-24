/*
 * 05_pointers_basics.c —— 指针基础
 *
 * 一句话理解指针: 指针是一个变量, 它的值是"另一个变量的地址"。
 *   &x  -> 取出 x 的地址
 *   *p  -> 顺着 p 里的地址找到那块内存(解引用)
 *
 * 指针是 C 的分水岭, 这里的每个小实验都建议动手改一改再跑一遍。
 */
#include <stdio.h>
#include <stddef.h>

/* 值传递: 形参只是实参的副本, 改它不影响外面 */
static void swap_by_value(int a, int b)
{
    int t = a;
    a = b;
    b = t;
    printf("   函数内部: a=%d b=%d (出了函数就消失了)\n", a, b);
}

/* 指针传递: 形参拿到了地址, 可以通过地址改外面的变量 */
static void swap_by_pointer(int *a, int *b)
{
    int t = *a;         /* 把 a 指向的值存起来 */
    *a = *b;            /* 把 b 指向的值写进 a 指向的位置 */
    *b = t;
}

/* 通过指针“返回”多个值: C 的函数只能 return 一个, 但可以修改指针所指的内存 */
static void divmod(int dividend, int divisor, int *quotient, int *remainder)
{
    if (divisor == 0) {
        *quotient = 0;
        *remainder = 0;
        return;
    }
    *quotient  = dividend / divisor;
    *remainder = dividend % divisor;
}

int main(void)
{
    /* ==================== 1. 地址与解引用 ==================== */
    puts("========== 1. 地址与解引用 ==========");

    int  n = 42;
    int *p = &n;                    /* p 里存的是 n 的地址 */

    printf("n 的值         = %d\n", n);
    printf("n 的地址       = %p\n", (void *)&n);
    printf("p 的值(=地址)  = %p\n", (void *)p);
    printf("p 自己的地址   = %p\n", (void *)&p);
    printf("*p (解引用)    = %d\n", *p);
    printf("sizeof(p)      = %zu  <-- 指针大小和它指向的类型无关\n", sizeof(p));

    *p = 100;                       /* 通过 p 修改 n */
    printf("执行 *p = 100 之后, n = %d\n", n);

    n = 7;                          /* 反过来改 n, *p 也跟着变(本来就是同一块内存) */
    printf("执行 n = 7 之后, *p = %d\n", *p);

    /* ==================== 2. 为什么必须传指针 ==================== */
    puts("\n========== 2. 值传递 vs 指针传递 ==========");

    int x = 1, y = 2;
    swap_by_value(x, y);
    printf("  swap_by_value 之后: x=%d y=%d   <-- 没变!\n", x, y);

    swap_by_pointer(&x, &y);
    printf("  swap_by_pointer 之后: x=%d y=%d  <-- 变了\n", x, y);

    int q = 0, r = 0;
    divmod(17, 5, &q, &r);
    printf("  divmod(17,5) -> 商=%d 余=%d  (用指针一次拿回两个结果)\n", q, r);

    /* ==================== 3. 指针的步长 ==================== */
    puts("\n========== 3. 指针算术: 走一步到底跨多少字节 ==========");

    int    arr[4] = {0, 11, 22, 33};
    int   *pi = arr;
    char  *pc = (char *)arr;
    double *pd = (double *)arr;     /* 只是演示地址差, 不解引用 */

    printf("int*    第 0 个 = %p, 第 1 个 = %p\n", (void *)pi, (void *)(pi + 1));
    printf("char*   第 0 个 = %p, 第 1 个 = %p\n", (void *)pc, (void *)(pc + 1));
    printf("double* 第 0 个 = %p, 第 1 个 = %p\n", (void *)pd, (void *)(pd + 1));
    printf("sizeof 分别是 %zu / %zu / %zu 字节\n",
           sizeof(int), sizeof(char), sizeof(double));

    /* 指针相减得到的是“元素个数”, 不是字节数 */
    ptrdiff_t dist = &arr[3] - &arr[0];
    printf("&arr[3] - &arr[0] = %td  <-- 结果是元素个数(3), 不是 12 字节\n", dist);

    /* ==================== 4. NULL 与野指针 ==================== */
    puts("\n========== 4. NULL 与野指针 ==========");

    int *good = NULL;               /* 好习惯: 暂时不用就置 NULL */
    if (good == NULL) {
        puts("good 是 NULL, 用之前一定要判空, 解引用 NULL 必然崩溃");
    }
    printf("good = %p\n", (void *)good);

    puts("常见的两类坏指针:");
    puts("  1) 野指针: int *p; 没初始化, 里面是随机地址 —— 解引用等于随机破坏内存");
    puts("  2) 悬垂指针: free(p) 之后又用 p —— use-after-free, 表现为随机崩溃");

    puts("\n正确的生命周期管理写法:");
    puts("  int *p = malloc(sizeof(int));");
    puts("  if (p == NULL) { 处理失败 }");
    puts("  *p = 1;");
    puts("  free(p);");
    puts("  p = NULL;      // 立刻置空, 避免后续误用");

    /* ==================== 5. const 与指针的三种组合 ==================== */
    puts("\n========== 5. const 和指针 ==========");

    int a = 10, b = 20;

    const int *p1 = &a;             /* 指向的内容不能改, 指针本身可以改 */
    int *const p2 = &a;             /* 指针本身不能改, 指向的内容可以改 */
    const int *const p3 = &a;       /* 都不能改 */

    p1 = &b;
    printf("p1 换了指向, 现在 *p1 = %d\n", *p1);

    *p2 = 30;
    printf("通过 p2 改值, a = %d\n", a);

    printf("p3 只读: *p3 = %d\n", *p3);

    puts("读法技巧: 从右往左读 —— const int *p 是 \"p is a pointer to const int\"");
    puts("  不能做: *p1 = 1;   p2 = &b;   *p3 = 1;   p3 = &b;");

    /* ==================== 6. 多级指针 ==================== */
    puts("\n========== 6. 多级指针 ==========");

    int  value = 7;
    int *pv    = &value;            /* 一级指针 */
    int **ppv  = &pv;               /* 二级指针: 指向“指针变量”的指针 */

    printf("value = %d, *pv = %d, **ppv = %d\n", value, *pv, **ppv);

    **ppv = 77;                     /* 按两次解引用就能改到 value */
    printf("执行 **ppv = 77 之后 value = %d\n", value);

    *ppv = &b;                      /* 改一级指针自己的指向 */
    printf("执行 *ppv = &b 之后, *pv = %d (pv 现在指向 b 了)\n", *pv);

    puts("二级指针典型用途: 函数里修改调用者的指针本身, 例如链表删除头结点。");

    /* ==================== 7. 指针 vs 数组名 ==================== */
    puts("\n========== 7. 一个小对比 ==========");
    printf("sizeof(arr)  = %zu  <-- 数组是 4 个 int\n", sizeof(arr));
    printf("sizeof(pi)   = %zu  <-- 指针只是一个地址\n", sizeof(pi));
    printf("arr == &arr[0] ? %s\n", (arr == &arr[0]) ? "是" : "否");
    printf("arr 的值 = %p, &arr[0] = %p\n", (void *)arr, (void *)&arr[0]);
    return 0;
}
