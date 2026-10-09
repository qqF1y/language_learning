/*
 * 07_functions_recursion.c —— 函数与递归
 *
 * 函数是 C 唯一的代码复用单位(没有类、没有命名空间)。
 * 递归 = 函数自己调用自己, 关键是想清楚两件事:
 *   1) 递归出口(边界条件) —— 什么时候停
 *   2) 递推关系             —— 大问题怎么变成小问题
 */
#include <stdio.h>

/* ==================== 函数声明(原型) ==================== */
/* 放在使用之前, 编译器才知道这些函数的参数和返回值类型。
 * 把声明写进 .h 头文件, 就是 18_multi_file 里做的事。 */
int  add(int a, int b);
void swap_by_value(int a, int b);
void swap_by_pointer(int *a, int *b);
int  counter_next(void);
void counter_reset(void);
long long factorial(int n);
long long fib_rec(int n);
long long fib_iter(int n);
long long fib_memo(int n);
int  gcd(int a, int b);
int  lcm(int a, int b);
void hanoi(int n, char from, char aux, char to, int *steps);
void print_reverse(const char *s);
int  sum_digits(int n);
int  is_sorted_rec(const int *arr, size_t n);

/* ==================== 函数定义 ==================== */
int add(int a, int b)
{
    return a + b;
}

/* 值传递: a、b 是副本 */
void swap_by_value(int a, int b)
{
    int t = a;
    a = b;
    b = t;
    printf("   函数内部: a=%d b=%d  <-- 只是副本被改了\n", a, b);
}

/* 指针传递: 通过地址直接操作调用者的内存 */
void swap_by_pointer(int *a, int *b)
{
    int t = *a;
    *a = *b;
    *b = t;
}

/* static 局部变量: 只初始化一次, 生命周期贯穿整个程序
 * (存放在静态存储区, 不是栈上, 所以函数返回后值还在) */
int counter_next(void)
{
    static int count = 0;
    return ++count;
}

void counter_reset(void)
{
    /* 这里不能直接改 count, 它在另一个函数的作用域里。
     * 想重置就得另想办法(比如用文件级全局变量或传指针)。 */
    puts("   (想重置 static 局部变量, 得改用全局变量或传入指针)");
}

/* ==================== 递归 ==================== */

/* 阶乘: n! = n * (n-1)!, 出口是 n <= 1 */
long long factorial(int n)
{
    if (n <= 1) {
        return 1;                       /* 递归出口, 必须有! */
    }
    return n * factorial(n - 1);        /* 递推 */
}

/* 斐波那契(朴素递归): 重复计算极多, O(2^n), n=45 就要等好几秒
 * 但它是理解"递归树"最好的例子 */
long long fib_rec(int n)
{
    if (n < 2) {
        return n;
    }
    return fib_rec(n - 1) + fib_rec(n - 2);
}

/* 斐波那契(迭代): O(n), 实际项目用这个 */
long long fib_iter(int n)
{
    long long a = 0, b = 1;
    for (int i = 0; i < n; i++) {
        long long next = a + b;
        a = b;
        b = next;
    }
    return a;
}

/* 斐波那契(记忆化): 用静态数组缓存, 把递归从 O(2^n) 降到 O(n) */
long long fib_memo(int n)
{
    static long long cache[93] = {0};    /* 0..92, 再大就溢出 long long 了 */
    static int inited = 0;

    if (n < 0 || n > 92) {
        return -1;
    }
    if (!inited) {                       /* 只初始化一次 */
        cache[0] = 0;
        cache[1] = 1;
        inited = 1;
    }
    if (cache[n] != 0 || n == 0) {
        return cache[n];
    }
    cache[n] = fib_memo(n - 1) + fib_memo(n - 2);
    return cache[n];
}

/* 辗转相除法(欧几里得算法)求最大公约数, 递归写法极其简洁 */
int gcd(int a, int b)
{
    return b == 0 ? a : gcd(b, a % b);
}

/* 最小公倍数 = 两数之积 / 最大公约数 (先除后乘, 避免溢出) */
int lcm(int a, int b)
{
    return a / gcd(a, b) * b;
}

/* 汉诺塔: 递归的教科书例子, 移动次数 = 2^n - 1 */
void hanoi(int n, char from, char aux, char to, int *steps)
{
    if (n == 1) {
        printf("    把盘 %d 从 %c 移到 %c\n", n, from, to);
        (*steps)++;
        return;
    }
    hanoi(n - 1, from, to, aux, steps);          /* 先把上面 n-1 个挪到中转柱 */
    printf("    把盘 %d 从 %c 移到 %c\n", n, from, to);   /* 把最大的挪到目标柱 */
    (*steps)++;
    hanoi(n - 1, aux, from, to, steps);          /* 再把 n-1 个从中转柱挪到目标柱 */
}

/* 递归反转打印字符串: 先递归到末尾, 回溯时再打印 */
void print_reverse(const char *s)
{
    if (*s == '\0') {
        return;                       /* 出口: 到字符串结尾 */
    }
    print_reverse(s + 1);             /* 先处理后面的 */
    putchar(*s);                      /* 回溯时打印, 顺序自然反了 */
}

/* 求各位数字之和: 1234 -> 1+2+3+4 = 10 */
int sum_digits(int n)
{
    if (n < 10) {
        return n;
    }
    return n % 10 + sum_digits(n / 10);
}

/* 递归判断数组是否有序 */
int is_sorted_rec(const int *arr, size_t n)
{
    if (n < 2) {
        return 1;                     /* 出口: 只剩一个元素, 当然有序 */
    }
    if (arr[0] > arr[1]) {
        return 0;
    }
    return is_sorted_rec(arr + 1, n - 1);
}

int main(void)
{
    /* ==================== 1. 返回值与参数 ==================== */
    puts("========== 1. 函数基本形态 ==========");
    printf("add(3, 4) = %d\n", add(3, 4));

    puts("\n-- 值传递 --");
    int x = 1, y = 2;
    swap_by_value(x, y);
    printf("   调用后: x=%d y=%d  (没变)\n", x, y);

    puts("-- 指针传递 --");
    swap_by_pointer(&x, &y);
    printf("   调用后: x=%d y=%d  (变了)\n", x, y);

    /* ==================== 2. static 局部变量 ==================== */
    puts("\n========== 2. static 局部变量 ==========");
    for (int i = 0; i < 3; i++) {
        printf("   第 %d 次调用 counter_next() = %d\n", i + 1, counter_next());
    }
    counter_reset();

    /* ==================== 3. 递归: 阶乘 ==================== */
    puts("\n========== 3. 递归: 阶乘 ==========");
    for (int i = 0; i <= 10; i++) {
        printf("   %2d! = %lld\n", i, factorial(i));
    }
    puts("   注意: 20! 是 2.4e18, 还在 long long 范围; 21! 就溢出了。");

    /* ==================== 4. 递归 vs 迭代: 斐波那契 ==================== */
    puts("\n========== 4. 斐波那契: 三种实现对比 ==========");
    printf("   前 15 项(迭代): ");
    for (int i = 0; i < 15; i++) {
        printf("%lld ", fib_iter(i));
    }
    putchar('\n');

    printf("   fib_rec(30)  = %lld  (朴素递归, 算了上百万次调用)\n", fib_rec(30));
    printf("   fib_memo(30) = %lld  (记忆化, 每个 n 只算一次)\n", fib_memo(30));
    printf("   fib_iter(30) = %lld  (迭代)\n", fib_iter(30));
    printf("   fib_iter(90) = %lld  (迭代能做很大)\n", fib_iter(90));

    /* ==================== 5. 递归: 最大公约数 ==================== */
    puts("\n========== 5. 最大公约数 / 最小公倍数 ==========");
    printf("   gcd(48, 18) = %d, lcm(48, 18) = %d\n", gcd(48, 18), lcm(48, 18));
    printf("   gcd(1071, 462) = %d  <-- 欧几里得算法的经典例子\n", gcd(1071, 462));

    /* ==================== 6. 递归: 汉诺塔 ==================== */
    puts("\n========== 6. 递归: 汉诺塔(3 个盘) ==========");
    int steps = 0;
    hanoi(3, 'A', 'B', 'C', &steps);
    printf("   共移动 %d 次 (= 2^3 - 1)\n", steps);

    /* ==================== 7. 递归: 字符串与数字 ==================== */
    puts("\n========== 7. 递归处理字符串和数字 ==========");
    printf("   反转打印 \"hello\": ");
    print_reverse("hello");
    putchar('\n');

    printf("   sum_digits(12345) = %d\n", sum_digits(12345));

    int sorted[5]     = {1, 3, 5, 7, 9};
    int not_sorted[5] = {1, 3, 2, 7, 9};
    printf("   {1,3,5,7,9} 有序? %s\n", is_sorted_rec(sorted, 5) ? "是" : "否");
    printf("   {1,3,2,7,9} 有序? %s\n", is_sorted_rec(not_sorted, 5) ? "是" : "否");

    /* ==================== 8. 递归的代价 ==================== */
    puts("\n========== 8. 使用递归的注意事项 ==========");
    puts("   1. 一定要有出口, 否则栈溢出(Stack Overflow)");
    puts("   2. 每层调用都要占栈空间, 默认栈约 8MB, 递归太深会崩");
    puts("   3. 有大量重复子问题时, 加记忆化或改写成迭代");
    puts("   4. 能用简单循环解决的, 优先用循环: 更快、更好调试");
    return 0;
}
