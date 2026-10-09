/*
 * 15_sort_search.c —— 经典排序与查找算法
 *
 * 排序: 冒泡 / 选择 / 插入 / 快速 / 库函数 qsort
 * 查找: 线性 / 二分(迭代+递归) / 库函数 bsearch
 *
 * 最后有一段性能对比, 用 clock() 实测, 直观感受 O(n^2) 和 O(n log n) 的差距。
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SMALL_N  12         /* 演示用的小数组 */
#define MID_N    6000       /* O(n^2) 算法的规模 */
#define BIG_N    200000     /* 快速排序/qsort 的规模 */

static unsigned long g_ops = 0;     /* 统计比较次数, 用来对比算法效率 */

static void swap_int(int *a, int *b)
{
    int t = *a;
    *a = *b;
    *b = t;
    g_ops++;
}

static void fill_random(int *a, size_t n, unsigned int seed)
{
    srand(seed);
    for (size_t i = 0; i < n; i++) {
        a[i] = rand();
    }
}

static void copy_array(int *dst, const int *src, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

static void print_array(const char *tag, const int *a, size_t n)
{
    /* 注意: 中文字符在 UTF-8 里占 3 个字节, 用 %-20s 这类宽度控制会对不齐,
     * 所以这里不补空格, 靠 "标签: 值" 的格式保证可读性 */
    printf("  %s: ", tag);
    for (size_t i = 0; i < n; i++) {
        printf("%d ", a[i]);
    }
    putchar('\n');
}

static int is_sorted(const int *a, size_t n)
{
    for (size_t i = 1; i < n; i++) {
        if (a[i - 1] > a[i]) {
            return 0;
        }
    }
    return 1;
}

static double elapsed(clock_t start)
{
    return (double)(clock() - start) / CLOCKS_PER_SEC;
}

/* ==================== 冒泡排序 ==================== */
/* 每一轮把当前最大的"冒"到末尾。稳定排序。 */
static void bubble_sort(int *a, size_t n)
{
    for (size_t i = 0; i + 1 < n; i++) {
        int swapped = 0;
        for (size_t j = 0; j + 1 < n - i; j++) {
            g_ops++;
            if (a[j] > a[j + 1]) {
                swap_int(&a[j], &a[j + 1]);
                swapped = 1;
            }
        }
        if (!swapped) {
            break;                  /* 本轮没交换, 说明已经有序, 提前退出 */
        }
    }
}

/* ==================== 选择排序 ==================== */
/* 每轮从未排序区间里挑最小的, 放到已排序区间的末尾。交换次数最少。 */
static void selection_sort(int *a, size_t n)
{
    for (size_t i = 0; i + 1 < n; i++) {
        size_t min = i;
        for (size_t j = i + 1; j < n; j++) {
            g_ops++;
            if (a[j] < a[min]) {
                min = j;
            }
        }
        if (min != i) {
            swap_int(&a[i], &a[min]);
        }
    }
}

/* ==================== 插入排序 ==================== */
/* 像抓扑克牌: 把新牌插到前面已排好的序列里。数据接近有序时非常快。 */
static void insertion_sort(int *a, size_t n)
{
    for (size_t i = 1; i < n; i++) {
        int    key = a[i];
        size_t j   = i;
        while (j > 0 && a[j - 1] > key) {
            g_ops++;
            a[j] = a[j - 1];
            j--;
        }
        g_ops++;
        a[j] = key;
    }
}

/* ==================== 快速排序 ==================== */
/* 分治: 选一个基准, 把小的放左边、大的放右边, 再对两边递归。
 * 平均 O(n log n), 是实践中最快的通用排序。 */
static void quick_sort(int *a, int lo, int hi)
{
    if (lo >= hi) {
        return;                     /* 区间里不足 2 个元素, 不用排 */
    }

    int pivot = a[lo + (hi - lo) / 2];      /* 取中间值当基准, 避免有序数组退化 */
    int i = lo, j = hi;

    while (i <= j) {
        while (a[i] < pivot) { i++; g_ops++; }
        while (a[j] > pivot) { j--; g_ops++; }
        if (i <= j) {
            swap_int(&a[i], &a[j]);
            i++;
            j--;
        }
    }
    /* 结束时: [lo, j] 都不大于基准, [i, hi] 都不小于基准 */
    quick_sort(a, lo, j);
    quick_sort(a, i, hi);
}

/* ==================== qsort 的比较函数 ==================== */
static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);       /* 避免 x - y 溢出 */
}

/* ==================== 查找 ==================== */

/* 线性查找: O(n), 不要求数据有序 */
static int linear_search(const int *a, size_t n, int key)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] == key) {
            return (int)i;
        }
    }
    return -1;                      /* -1 表示没找到 */
}

/* 二分查找(迭代): O(log n), 必须已排序 */
static int binary_search(const int *a, size_t n, int key)
{
    size_t lo = 0, hi = n;          /* 在左闭右开区间 [lo, hi) 里找 */

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;    /* 这样写不会溢出 */
        if (a[mid] == key) {
            return (int)mid;
        }
        if (a[mid] < key) {
            lo = mid + 1;           /* 目标在右半边 */
        } else {
            hi = mid;               /* 目标在左半边 */
        }
    }
    return -1;
}

/* 二分查找(递归): 逻辑一样, 体会两种写法 */
static int binary_search_rec(const int *a, int lo, int hi, int key)
{
    if (lo > hi) {
        return -1;
    }
    int mid = lo + (hi - lo) / 2;
    if (a[mid] == key) {
        return mid;
    }
    if (a[mid] < key) {
        return binary_search_rec(a, mid + 1, hi, key);
    }
    return binary_search_rec(a, lo, mid - 1, key);
}

/* ==================== 演示 ==================== */
static void demo_small(void)
{
    puts("========== 1. 小数组上手 ==========");

    int base[SMALL_N] = {42, 7, 19, 88, 3, 56, 21, 9, 74, 30, 15, 61};
    int work[SMALL_N];

    print_array("原始", base, SMALL_N);

    copy_array(work, base, SMALL_N);
    g_ops = 0;
    bubble_sort(work, SMALL_N);
    print_array("冒泡排序", work, SMALL_N);
    printf("  比较+交换 %lu 次\n\n", g_ops);

    copy_array(work, base, SMALL_N);
    g_ops = 0;
    selection_sort(work, SMALL_N);
    print_array("选择排序", work, SMALL_N);
    printf("  比较+交换 %lu 次\n\n", g_ops);

    copy_array(work, base, SMALL_N);
    g_ops = 0;
    insertion_sort(work, SMALL_N);
    print_array("插入排序", work, SMALL_N);
    printf("  比较+交换 %lu 次\n\n", g_ops);

    copy_array(work, base, SMALL_N);
    g_ops = 0;
    quick_sort(work, 0, SMALL_N - 1);
    print_array("快速排序", work, SMALL_N);
    printf("  比较+交换 %lu 次\n", g_ops);
    printf("  结果有序? %s\n", is_sorted(work, SMALL_N) ? "是" : "否");
}

static void demo_search(void)
{
    puts("\n========== 2. 查找 ==========");

    int a[10] = {3, 7, 9, 15, 21, 30, 42, 56, 74, 88};     /* 已排序 */
    size_t n = sizeof(a) / sizeof(a[0]);

    print_array("有序数组", a, n);

    int keys[] = {21, 30, 100};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        int key = keys[i];
        int lin = linear_search(a, n, key);
        int bin = binary_search(a, n, key);
        int rec = binary_search_rec(a, 0, (int)n - 1, key);

        printf("  查找 %3d -> 线性:%2d  二分(迭代):%2d  二分(递归):%2d  %s\n",
               key, lin, bin, rec, bin < 0 ? "(不存在)" : "");
    }
    puts("\n  二分查找的前提是数据有序! 无序数据必须先排序。");
    puts("  二分每次把范围砍一半: 100 万个数据最多比较 20 次。");

    /* 库函数 bsearch 用法与 qsort 类似 */
    int key = 56;
    const int *hit = bsearch(&key, a, n, sizeof(int), cmp_int);
    if (hit != NULL) {
        printf("  bsearch(56) 找到, 下标 %td\n", hit - a);
    }
}

static void demo_benchmark(void)
{
    puts("\n========== 3. 性能对比(实测) ==========");

    int *base = malloc(BIG_N * sizeof(int));
    int *work = malloc(BIG_N * sizeof(int));
    if (base == NULL || work == NULL) {
        free(base);
        free(work);
        puts("  内存不足, 跳过");
        return;
    }

    fill_random(base, BIG_N, 2024u);

    clock_t t0;

    t0 = clock();
    copy_array(work, base, MID_N);
    g_ops = 0;
    bubble_sort(work, MID_N);
    printf("  冒泡排序 (n=%6d): %6.3f 秒, 操作 %10lu 次, 有序=%d\n",
           MID_N, elapsed(t0), g_ops, is_sorted(work, MID_N));

    t0 = clock();
    copy_array(work, base, MID_N);
    g_ops = 0;
    selection_sort(work, MID_N);
    printf("  选择排序 (n=%6d): %6.3f 秒, 操作 %10lu 次, 有序=%d\n",
           MID_N, elapsed(t0), g_ops, is_sorted(work, MID_N));

    t0 = clock();
    copy_array(work, base, MID_N);
    g_ops = 0;
    insertion_sort(work, MID_N);
    printf("  插入排序 (n=%6d): %6.3f 秒, 操作 %10lu 次, 有序=%d\n",
           MID_N, elapsed(t0), g_ops, is_sorted(work, MID_N));

    t0 = clock();
    copy_array(work, base, MID_N);
    g_ops = 0;
    quick_sort(work, 0, MID_N - 1);
    printf("  快速排序 (n=%6d): %6.3f 秒, 操作 %10lu 次, 有序=%d\n",
           MID_N, elapsed(t0), g_ops, is_sorted(work, MID_N));

    t0 = clock();
    copy_array(work, base, BIG_N);
    g_ops = 0;
    quick_sort(work, 0, BIG_N - 1);
    printf("  快速排序 (n=%6d): %6.3f 秒, 操作 %10lu 次, 有序=%d\n",
           BIG_N, elapsed(t0), g_ops, is_sorted(work, BIG_N));

    t0 = clock();
    copy_array(work, base, BIG_N);
    g_ops = 0;
    qsort(work, BIG_N, sizeof(int), cmp_int);
    printf("  库函数 qsort(n=%5d): %6.3f 秒, 操作       ---  次, 有序=%d\n",
           BIG_N, elapsed(t0), is_sorted(work, BIG_N));

    puts("\n  规模从 6000 涨到 200000(33 倍):");
    puts("    O(n^2) 算法的耗时涨约 1100 倍, O(n log n) 只涨约 40 倍。");
    puts("  所以大数据量下, 选对算法比微调代码重要得多。");

    free(base);
    free(work);
}

static void demo_stability_note(void)
{
    puts("\n========== 4. 稳定性与选择建议 ==========");
    puts("  稳定排序: 相等元素的相对顺序保持不变(冒泡/插入/归并/计数)");
    puts("  不稳定:   可能改变相等元素的顺序(选择/快速/堆排序)");
    puts("  什么时候需要稳定? 例如先按姓名排, 再按班级排, 希望同班内仍按姓名有序。");
    putchar('\n');
    puts("  选择建议:");
    puts("    - n 很小(<50): 插入排序, 代码短又够快");
    puts("    - 通用场景:      qsort / 快速排序");
    puts("    - 数据接近有序:  插入排序(接近 O(n))");
    puts("    - 要求稳定:      归并排序");
    puts("    - 整数范围小:    计数排序(线性时间)");
    puts("    - 实际写代码:    直接用 qsort, 别自己造轮子");
}

int main(void)
{
    demo_small();
    demo_search();
    demo_benchmark();
    demo_stability_note();
    return 0;
}
