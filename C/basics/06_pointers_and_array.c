/*
 * 06_pointers_and_array.c —— 指针与数组/字符串/内存
 *
 * 记住这三条等价关系, 指针和数组就通了:
 *     arr[i]   ==   *(arr + i)
 *     &arr[i]  ==   arr + i
 *     函数参数里的 int arr[]  其实就是  int *arr
 */
#include <stdio.h>
#include <string.h>     /* memcpy, memset, strlen */

/* ---------- 数组作为函数参数: 会“退化”成指针 ---------- */
/* 这里的 int arr[] 和 int *arr 完全等价, 都只是拿到首地址, 不知道长度! */
static int sum_array(const int *arr, size_t n)
{
    int s = 0;
    for (size_t i = 0; i < n; i++) {
        s += arr[i];
    }
    return s;
}

static void fill(int *arr, size_t n, int value)
{
    for (size_t i = 0; i < n; i++) {
        arr[i] = value;
    }
    /* 也可以写成指针形式, 效果完全一样:
     *   for (int *p = arr; p < arr + n; p++) *p = value;
     */
}

static void print_array(const char *tag, const int *arr, size_t n)
{
    printf("%-14s: ", tag);
    for (size_t i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    putchar('\n');
}

/* ---------- 用指针实现字符串库函数 ---------- */
static size_t my_strlen(const char *s)
{
    const char *p = s;
    while (*p != '\0') {
        p++;
    }
    return (size_t)(p - s);         /* 指针相减 = 走了多少个字符 */
}

static char *my_strcpy(char *dst, const char *src)
{
    char *save = dst;               /* 记住起点, 最后返回它(便于链式调用) */
    while ((*dst++ = *src++) != '\0') {
        /* 空循环体: 赋值、判断、两个指针都自增 */
    }
    return save;
}

static void reverse_in_place(char *s)
{
    if (s == NULL) return;
    char *left = s;
    char *right = s + strlen(s);    /* 指向 '\0' */
    if (left == right) return;
    right--;                        /* 指向最后一个字符 */

    while (left < right) {
        char t = *left;
        *left++ = *right;
        *right-- = t;
    }
}

/* ---------- 通过指针返回多个结果 ---------- */
static void min_max(const int *arr, size_t n, int *out_min, int *out_max)
{
    if (n == 0) {
        return;
    }
    int mn = arr[0], mx = arr[0];
    for (size_t i = 1; i < n; i++) {
        if (arr[i] < mn) mn = arr[i];
        if (arr[i] > mx) mx = arr[i];
    }
    *out_min = mn;
    *out_max = mx;
}

/* ---------- 返回指针的函数: 返回数组内部元素的地址是安全的 ---------- */
static int *find_element(int *arr, size_t n, int key)
{
    for (size_t i = 0; i < n; i++) {
        if (arr[i] == key) {
            return &arr[i];         /* 返回的是调用者数组里的地址, 生命周期没问题 */
        }
    }
    return NULL;                    /* 没找到返回 NULL, 调用者必须判空 */
}

int main(void)
{
    /* ==================== 1. 下标法 == 指针法 ==================== */
    puts("========== 1. arr[i] 就是 *(arr + i) ==========");

    int a[5] = {10, 20, 30, 40, 50};

    printf("a[2]        = %d\n", a[2]);
    printf("*(a + 2)    = %d\n", *(a + 2));
    printf("*(2 + a)    = %d  <-- 加法可交换, 证明 [] 只是语法糖\n", *(2 + a));
    printf("2[a]        = %d  <-- 合法但千万别这么写\n", 2[a]);
    printf("&a[2]       = %p, a + 2 = %p  (相同)\n", (void *)&a[2], (void *)(a + 2));

    /* 用指针遍历 */
    puts("用指针遍历数组:");
    for (int *p = a; p < a + 5; p++) {
        printf("  地址 %p -> 值 %d\n", (void *)p, *p);
    }

    /* ==================== 2. 数组名不是指针 ==================== */
    puts("\n========== 2. 数组名和指针的区别 ==========");
    printf("sizeof(a) = %zu  (整个数组 5*4=20 字节)\n", sizeof(a));
    printf("数组名不能自增: a++ 是错的; 但指针可以\n");

    int *pa = a;
    pa++;                           /* 合法: 指针变量可以改 */
    printf("pa++ 之后 *pa = %d\n", *pa);

    /* ==================== 3. 数组做参数 ==================== */
    puts("\n========== 3. 数组做参数 ==========");

    print_array("原始", a, 5);
    fill(a, 5, 7);
    print_array("fill 后", a, 5);

    int b[6] = {5, 3, 9, 1, 7, 2};
    print_array("b", b, 6);
    printf("sum_array(b) = %d\n", sum_array(b, 6));

    int mn = 0, mx = 0;
    min_max(b, 6, &mn, &mx);
    printf("min=%d max=%d\n", mn, mx);

    printf("sizeof(a) 在 main 里是 %zu, 但在函数里 sizeof(arr) 只有 %zu —— "
           "所以长度必须额外传进来\n", sizeof(a), sizeof(int *));

    /* ==================== 4. 指针数组 vs 数组指针 ==================== */
    puts("\n========== 4. 指针数组 vs 数组指针 ==========");

    /* 指针数组: 是“数组”, 元素是指针。读法: p1 是 array of pointer to char */
    const char *names[3] = {"Tom", "Jerry", "Spike"};
    for (size_t i = 0; i < 3; i++) {
        printf("  名字[%zu] = %-6s (地址 %p)\n", i, names[i], (void *)names[i]);
    }

    /* 数组指针: 是“指针”, 指向一个数组。读法: p2 是 pointer to array of int[4] */
    int row1[4] = {1, 2, 3, 4};
    int (*rowp)[4] = &row1;         /* 注意 & 和括号 */
    printf("  (*rowp)[2] = %d\n", (*rowp)[2]);

    /* 指向二维数组的“行指针”, 是遍历二维数组的经典写法 */
    int grid[3][4] = {
        { 1,  2,  3,  4},
        { 5,  6,  7,  8},
        { 9, 10, 11, 12},
    };
    puts("用行指针遍历二维数组:");
    for (int (*row)[4] = grid; row < grid + 3; row++) {
        printf("  ");
        for (int *col = *row; col < *row + 4; col++) {
            printf("%3d ", *col);
        }
        putchar('\n');
    }

    /* ==================== 5. 字符串与指针 ==================== */
    puts("\n========== 5. 字符串与指针 ==========");

    char s[32] = "hello";
    printf("my_strlen(\"%s\") = %zu\n", s, my_strlen(s));

    char dst[32];
    my_strcpy(dst, s);
    printf("my_strcpy 结果 = %s\n", dst);

    char rev[] = "abcdef";
    reverse_in_place(rev);
    printf("reverse_in_place(\"abcdef\") = %s\n", rev);

    /* 字符串数组: 按指针遍历 */
    const char *words[] = {"apple", "banana", "cherry"};
    size_t n = sizeof(words) / sizeof(words[0]);
    for (size_t i = 0; i < n; i++) {
        const char *p = words[i];
        printf("  %-8s 首字符='%c' 长度=%zu\n", p, *p, strlen(p));
    }

    /* ==================== 6. void* 与内存函数 ==================== */
    puts("\n========== 6. void* 与 memcpy/memset ==========");

    int src[4] = {1, 2, 3, 4};
    int copy2[4];

    memset(copy2, 0, sizeof(copy2));            /* 全部按字节填 0 */
    memcpy(copy2, src, sizeof(src));            /* 按字节拷贝(不做重叠处理) */

    print_array("memcpy 结果", copy2, 4);

    double d = 3.14;
    unsigned char raw[sizeof(double)];
    memcpy(raw, &d, sizeof(d));                 /* 用 void* 观察任意数据的原始字节 */
    printf("double %.2f 的原始字节: ", d);
    for (size_t i = 0; i < sizeof(d); i++) {
        printf("%02X ", raw[i]);
    }
    putchar('\n');

    /* ==================== 7. 返回指针的函数 ==================== */
    puts("\n========== 7. 返回指针的函数 ==========");

    int data[6] = {11, 22, 33, 44, 55, 66};
    int *found = find_element(data, 6, 33);
    if (found != NULL) {
        printf("找到 33, 地址 %p, 下标 %td\n", (void *)found, found - data);
        *found = 333;                       /* 可以直接改原数组 */
        print_array("修改后", data, 6);
    }

    if (find_element(data, 6, 999) == NULL) {
        puts("没找到 999 —— 返回 NULL 是 C 的习惯做法");
    }

    puts("\n切记: 不要返回局部数组的地址! 例如");
    puts("  int *bad(void) { int local[4]; return local; }  // 函数一返回 local 就没了");
    return 0;
}
