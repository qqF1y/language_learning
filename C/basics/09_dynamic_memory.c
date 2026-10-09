/*
 * 09_dynamic_memory.c —— 动态内存管理
 *
 * 栈内存: 由编译器自动分配释放, 大小有限(默认约 8MB), 又小又快。
 * 堆内存: 由程序员手动 malloc / free, 可以很大, 但忘释放就是内存泄漏。
 *
 * 四个函数:
 *   malloc(n)        分配 n 字节, 内容不确定
 *   calloc(cnt, size) 分配 cnt*size 字节, 并清零
 *   realloc(p, n)    把 p 调整为 n 字节(可能搬家), 返回新地址
 *   free(p)          释放; free(NULL) 是安全的
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 1. 三个分配函数 ==================== */
static void demo_basic(void)
{
    puts("========== 1. malloc / calloc / free ==========");

    /* --- malloc: 只分配, 不清零 --- */
    int *p = malloc(sizeof(int));
    if (p == NULL) {                    /* 一定要判空! 内存可能分配失败 */
        perror("malloc");
        return;
    }
    *p = 42;
    printf("malloc 得到 1 个 int: *p = %d\n", *p);

    free(p);                            /* 归还给系统 */
    p = NULL;                           /* 立刻置空, 避免悬垂指针 */
    puts("free(p) 之后立刻 p = NULL, 之后误用会立即崩溃而不是随机出错");

    /* --- calloc: 分配并清零 --- */
    size_t n = 5;
    int *zeros = calloc(n, sizeof(int));
    if (zeros == NULL) {
        return;
    }
    printf("calloc 得到: ");
    for (size_t i = 0; i < n; i++) {
        printf("%d ", zeros[i]);        /* 全是 0 */
    }
    putchar('\n');

    /* 用 memset 也能达到同样效果 */
    int *memset_arr = malloc(n * sizeof(int));
    if (memset_arr != NULL) {
        memset(memset_arr, 0, n * sizeof(int));
        printf("memset 清零后: %d %d %d\n", memset_arr[0], memset_arr[1], memset_arr[2]);
        free(memset_arr);
    }

    free(zeros);

    /* --- realloc: 扩容 --- */
    size_t cap = 2;
    int *arr = malloc(cap * sizeof(int));
    if (arr == NULL) {
        return;
    }
    arr[0] = 1;
    arr[1] = 2;
    printf("扩容前: 地址 = %p, 容量 = %zu\n", (void *)arr, cap);

    cap = 10;
    int *bigger = realloc(arr, cap * sizeof(int));
    if (bigger == NULL) {
        /* realloc 失败时, 原来的 arr 仍然有效, 必须自己释放 */
        free(arr);
        return;
    }
    arr = bigger;                       /* 必须用返回值覆盖原指针! */
    printf("扩容后: 地址 = %p, 容量 = %zu  <-- 地址可能变了(搬到了新地方)\n",
           (void *)arr, cap);

    for (size_t i = 2; i < cap; i++) {
        arr[i] = (int)i + 1;            /* 新增的区域内容不确定, 必须自己写 */
    }
    printf("新内容: ");
    for (size_t i = 0; i < cap; i++) {
        printf("%d ", arr[i]);
    }
    putchar('\n');

    free(arr);
}

/* ==================== 2. 返回堆内存的函数 ==================== */
/* 谁分配、谁释放 —— 这条规矩一定要在函数注释里写清楚 */
static int *make_sequence(size_t n, int start)
{
    int *a = malloc(n * sizeof(int));
    if (a == NULL) {
        return NULL;                    /* 失败返回 NULL, 让调用者决定怎么办 */
    }
    for (size_t i = 0; i < n; i++) {
        a[i] = start + (int)i;
    }
    return a;                           /* 注意: 这里返回堆内存是安全的 */
}

static void demo_return_heap(void)
{
    puts("\n========== 2. 返回堆内存 ==========");

    int *seq = make_sequence(8, 100);
    if (seq == NULL) {
        return;
    }
    printf("make_sequence(8, 100) = ");
    for (size_t i = 0; i < 8; i++) {
        printf("%d ", seq[i]);
    }
    putchar('\n');

    free(seq);      /* 调用者负责释放 */
    puts("调用者负责 free —— 这是 C 里最容易搞混的地方");

    puts("\n对比: 千万不要返回局部数组的地址!");
    puts("  int *bad(void) { int local[4] = {1,2,3,4}; return local; }");
    puts("  函数一返回, local 所在的栈帧就失效了, 拿到的指针是悬垂的。");
}

/* ==================== 3. 动态字符串 ==================== */
/* 标准库有 strdup(), 但它属于 POSIX 而不是 C 标准, 所以自己写一个 */
static char *my_strdup(const char *s)
{
    size_t len = strlen(s) + 1;         /* +1 是给结尾的 '\0' */
    char *copy = malloc(len);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, s, len);
    return copy;
}

static void demo_string(void)
{
    puts("\n========== 3. 动态字符串 ==========");

    const char *src = "动态分配一段字符串";
    char *copy = my_strdup(src);
    if (copy == NULL) {
        return;
    }

    printf("原串: %s (地址 %p)\n", src, (void *)src);
    printf("副本: %s (地址 %p)\n", copy, (void *)copy);
    printf("strcmp 结果 = %d (0 表示内容相同)\n", strcmp(src, copy));

    free(copy);
}

/* ==================== 4. 动态扩容(手写 vector) ==================== */
static void demo_growing(void)
{
    puts("\n========== 4. 动态数组扩容 ==========");

    size_t cap = 4, len = 0;
    int *data = malloc(cap * sizeof(int));
    if (data == NULL) {
        return;
    }

    for (int v = 1; v <= 20; v++) {
        if (len == cap) {               /* 空间不够了, 翻倍扩容 */
            size_t new_cap = cap * 2;
            int *tmp = realloc(data, new_cap * sizeof(int));
            if (tmp == NULL) {
                free(data);
                return;
            }
            data = tmp;
            cap = new_cap;
            printf("  容量不够, 扩容到 %zu\n", cap);
        }
        data[len++] = v;
    }

    printf("  最终 %zu 个元素: ", len);
    for (size_t i = 0; i < len; i++) {
        printf("%d ", data[i]);
    }
    putchar('\n');
    puts("  容量翻倍(1,2,4,8...)让追加操作平均只要 O(1), 这就是 vector/ArrayList 的原理");

    free(data);
}

/* ==================== 5. 动态二维数组 ==================== */
static void demo_2d(void)
{
    puts("\n========== 5. 动态二维数组 ==========");

    size_t rows = 3, cols = 4;

    /* 先分配"行指针数组", 再给每一行单独分配 */
    int **m = malloc(rows * sizeof(int *));
    if (m == NULL) {
        return;
    }

    for (size_t i = 0; i < rows; i++) {
        m[i] = malloc(cols * sizeof(int));
        if (m[i] == NULL) {             /* 出错要把已经分配的都还回去 */
            for (size_t k = 0; k < i; k++) {
                free(m[k]);
            }
            free(m);
            return;
        }
    }

    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) {
            m[i][j] = (int)(i * cols + j);
        }
    }

    puts("  内容:");
    for (size_t i = 0; i < rows; i++) {
        printf("    ");
        for (size_t j = 0; j < cols; j++) {
            printf("%3d ", m[i][j]);
        }
        putchar('\n');
    }

    /* 释放顺序必须和分配顺序相反: 先释放每行, 再释放行指针数组 */
    for (size_t i = 0; i < rows; i++) {
        free(m[i]);
    }
    free(m);
    puts("  释放顺序: 先 free 每一行, 最后 free 行指针数组");

    /* 另一种写法: 一整块连续内存, 一次分配一次释放, 缓存更友好 */
    int *flat = malloc(rows * cols * sizeof(int));
    if (flat != NULL) {
        for (size_t i = 0; i < rows; i++) {
            for (size_t j = 0; j < cols; j++) {
                flat[i * cols + j] = (int)(i * 10 + j);
            }
        }
        printf("  连续内存版 m[1][2] = %d\n", flat[1 * cols + 2]);
        free(flat);                     /* 只需要一次 free */
    }
}

/* ==================== 6. 内存错误清单 ==================== */
static void demo_pitfalls(void)
{
    puts("\n========== 6. 常见内存错误 ==========");
    puts("  1. 忘记 free          -> 内存泄漏, 长时间运行的程序会把内存吃光");
    puts("  2. free 两次          -> 堆破坏, 崩溃位置往往离出错点很远");
    puts("  3. free 之后继续用     -> use-after-free, 最危险的漏洞来源");
    puts("  4. 越界读写            -> 堆溢出, 可能覆盖 malloc 的内部信息");
    puts("  5. 不判 malloc 返回值  -> 内存紧张时直接崩溃");
    puts("  6. 用 realloc 的返回值没接住 -> 旧内存泄漏, 新内存丢失");
    puts("  7. 把 malloc 的数当成 0 -> malloc 不清零! 要清零用 calloc");

    puts("\n  排查工具:");
    puts("    gcc -fsanitize=address,undefined -g xx.c   # 编译期插桩, 运行即报错, 首选");
    puts("    valgrind --leak-check=full ./a.out         # 无需重新编译");
}

int main(void)
{
    demo_basic();
    demo_return_heap();
    demo_string();
    demo_growing();
    demo_2d();
    demo_pitfalls();
    return 0;
}
