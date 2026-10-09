/*
 * 14_function_pointers.c —— 函数指针与回调
 *
 * 函数指针 = "存函数地址的变量"。
 * 有了它, C 才能做"把函数当参数传"这件事, 也就是回调(callback)。
 *
 * 声明读法: 从里往外读, 先看变量名和最近的括号
 *     int (*fp)(int, int);
 *         ^^^^  fp 是一个指针
 *              ^^^^^^^^^^  指向"接收 (int,int)、返回 int"的函数
 *
 * 对比: int *fp(int, int);   这是"返回 int* 的函数", 差别就在那对括号!
 */
#include <stdio.h>
#include <stdlib.h>

/* ==================== 被调用的函数们 ==================== */
static int op_add(int a, int b)    { return a + b; }
static int op_sub(int a, int b)    { return a - b; }
static int op_mul(int a, int b)    { return a * b; }
static int op_div(int a, int b)    { return b != 0 ? a / b : 0; }
static int op_mod(int a, int b)    { return b != 0 ? a % b : 0; }

/* typedef 起别名: 之后写函数指针就清爽多了 */
typedef int (*BinOp)(int, int);

/* ==================== 回调: 把函数当参数 ==================== */
static int double_it(int x) { return x * 2; }
static int square_it(int x) { return x * x; }
static int negate_it(int x) { return -x; }

/* 对一个数组的每个元素应用 fn, 结果写回原数组 */
static void map_array(int *arr, size_t n, int (*fn)(int))
{
    for (size_t i = 0; i < n; i++) {
        arr[i] = fn(arr[i]);
    }
}

/* 归约: 用 fn 把数组折叠成一个值 */
static int reduce_array(const int *arr, size_t n, int init, BinOp fn)
{
    int acc = init;
    for (size_t i = 0; i < n; i++) {
        acc = fn(acc, arr[i]);
    }
    return acc;
}

/* ==================== 用回调实现"可换比较规则"的排序 ==================== */
static void sort_with(int *arr, size_t n, int (*cmp)(int, int))
{
    for (size_t i = 0; i + 1 < n; i++) {
        for (size_t j = 0; j + 1 < n - i; j++) {
            if (cmp(arr[j], arr[j + 1]) > 0) {  /* cmp > 0 表示顺序不对 */
                int t = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = t;
            }
        }
    }
}

static int cmp_asc(int a, int b)  { return (a > b) - (a < b); }
static int cmp_desc(int a, int b) { return (b > a) - (b < a); }

/* ==================== qsort 需要的比较函数(签名是固定的) ==================== */
static int cmp_int_asc(const void *a, const void *b)
{
    int x = *(const int *)a;        /* 先把 void* 转成真正的类型 */
    int y = *(const int *)b;
    return (x > y) - (x < y);       /* 这样写不会溢出; 直接 x-y 在极值时会溢出 */
}

static int cmp_int_desc(const void *a, const void *b)
{
    return cmp_int_asc(b, a);       /* 反过来调一次就行 */
}

/* ==================== 函数指针数组 ==================== */
typedef struct {
    const char *name;               /* 运算符名字 */
    const char *symbol;
    BinOp       op;                 /* 对应的函数 */
} OpEntry;

/* ==================== 返回函数指针的函数 ==================== */
static BinOp op_lookup(const char *symbol)
{
    static const OpEntry table[] = {
        { "add", "+", op_add },
        { "sub", "-", op_sub },
        { "mul", "*", op_mul },
        { "div", "/", op_div },
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (table[i].symbol[0] == symbol[0]) {
            return table[i].op;
        }
    }
    return NULL;
}

static void print_array(const char *tag, const int *a, size_t n)
{
    printf("  %s: ", tag);
    for (size_t i = 0; i < n; i++) {
        printf("%d ", a[i]);
    }
    putchar('\n');
}

int main(void)
{
    /* ==================== 1. 基本用法 ==================== */
    puts("========== 1. 声明、赋值、调用 ==========");

    int (*fp)(int, int);            /* 未初始化的函数指针是野指针 */
    fp = op_add;                    /* 函数名本身就是地址, 不用写 & */
    printf("  fp(3, 4)    = %d\n", fp(3, 4));
    printf("  (*fp)(3, 4) = %d  <-- 两种写法完全等价\n", (*fp)(3, 4));

    fp = &op_sub;                   /* 写 & 也行, 效果一样 */
    printf("  op_sub(10, 3) = %d\n", fp(10, 3));

    printf("  sizeof(fp) = %zu  <-- 函数指针就是一个地址\n", sizeof(fp));

    /* ==================== 2. typedef 之后 ==================== */
    puts("\n========== 2. 用 typedef 让代码好看 ==========");

    BinOp f2 = op_mul;
    printf("  BinOp f2 = op_mul; f2(6, 7) = %d\n", f2(6, 7));

    /* 不用 typedef 的话, 声明函数指针类型的数组要写成这样: */
    int (*ops_raw[4])(int, int) = { op_add, op_sub, op_mul, op_div };
    printf("  裸写的函数指针数组: %d %d %d %d\n",
           ops_raw[0](10, 4), ops_raw[1](10, 4), ops_raw[2](10, 4), ops_raw[3](10, 4));

    /* ==================== 3. 分发表 ==================== */
    puts("\n========== 3. 函数指针数组做分发表(运算器) ==========");

    static const OpEntry table[] = {
        { "加法", "+", op_add },
        { "减法", "-", op_sub },
        { "乘法", "*", op_mul },
        { "除法", "/", op_div },
        { "取余", "%", op_mod },
    };

    int a = 12, b = 5;
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        printf("  %s: %d %s %d = %d\n", table[i].name, a, table[i].symbol, b,
               table[i].op(a, b));
    }
    puts("  这就是\"表驱动\"替代长串 if/else 或 switch 的经典手法。");

    /* ==================== 4. 返回函数指针 ==================== */
    puts("\n========== 4. 返回函数指针的函数 ==========");

    const char *symbols[] = { "+", "-", "*", "/", "?" };
    for (size_t i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++) {
        BinOp fn = op_lookup(symbols[i]);
        if (fn != NULL) {
            printf("  找到运算符 '%s': 10 %s 3 = %d\n",
                   symbols[i], symbols[i], fn(10, 3));
        } else {
            printf("  运算符 '%s' 不支持\n", symbols[i]);
        }
    }
    puts("  函数指针同样要判空, 用 NULL 表示\"没有\"。");

    /* ==================== 5. 回调: map ==================== */
    puts("\n========== 5. 回调: map ==========");

    int arr[6] = {1, 2, 3, 4, 5, 6};
    print_array("原始", arr, 6);

    map_array(arr, 6, double_it);
    print_array("每个元素乘 2", arr, 6);

    map_array(arr, 6, square_it);
    print_array("每个元素求平方", arr, 6);

    map_array(arr, 6, negate_it);
    print_array("每个元素取负", arr, 6);

    puts("  同一段循环代码, 换个函数就换一种行为 —— 这就是回调的威力。");

    /* ==================== 6. 回调: reduce 与自定义排序 ==================== */
    puts("\n========== 6. 回调: reduce 与自定义排序 ==========");

    int data[8] = {5, 2, 9, 1, 7, 3, 8, 4};
    print_array("原始", data, 8);

    printf("  求和 = %d\n", reduce_array(data, 8, 0, op_add));
    printf("  求积 = %d  (故意从 1 开始才有意义)\n", reduce_array(data, 8, 1, op_mul));

    sort_with(data, 8, cmp_asc);
    print_array("升序(cmp_asc)", data, 8);

    sort_with(data, 8, cmp_desc);
    print_array("降序(cmp_desc)", data, 8);

    /* ==================== 7. 标准库的 qsort ==================== */
    puts("\n========== 7. 库里最有名的回调: qsort ==========");

    int nums[10] = {42, 7, 19, 88, 3, 56, 21, 9, 74, 30};
    print_array("排序前", nums, 10);

    qsort(nums, 10, sizeof(int), cmp_int_asc);
    print_array("qsort 升序", nums, 10);

    qsort(nums, 10, sizeof(int), cmp_int_desc);
    print_array("qsort 降序", nums, 10);

    puts("  qsort 四个参数: 数组首地址, 元素个数, 每个元素字节数, 比较函数");
    puts("  比较函数返回 <0 / 0 / >0 分别表示 a 排在 b 前面 / 相同 / 后面");

    puts("\n========== 8. 使用注意 ==========");
    puts("  1. 函数指针必须初始化为有效函数或 NULL, 否则调用会崩溃");
    puts("  2. 函数指针类型必须完全匹配: 返回值、参数个数、参数类型都要一致");
    puts("  3. 回调函数必须是 static 或者外部可见的, 不要用嵌套函数(GCC 扩展)");
    puts("  4. 回调里传上下文指针(void *userdata)是常见套路, 例如:");
    puts("         void sort(void *base, size_t n, int (*cmp)(const void *, const void *, void *), void *ctx);");
    return 0;
}
