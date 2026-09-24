/*
 * 16_classic_puzzles.c —— C 语言经典练习题合集
 *
 * 这些是学 C 一定会遇到的题目。每个都尽量用最朴素、最好懂的方式写,
 * 注释说明思路。建议先看懂, 再合上代码自己写一遍。
 */
#include <stdio.h>
#include <string.h>

/* ==================== 1. 九九乘法表 ==================== */
static void puzzle_99_table(void)
{
    puts("========== 1. 九九乘法表 ==========");

    for (int i = 1; i <= 9; i++) {
        for (int j = 1; j <= i; j++) {          /* 内层到 i 为止, 形成三角形 */
            printf("%d*%d=%-3d", j, i, i * j);  /* %-3d 左对齐, 保证列整齐 */
        }
        putchar('\n');
    }
}

/* ==================== 2. 素数 ==================== */
static int is_prime(int n)
{
    if (n < 2) {
        return 0;                   /* 0 和 1 都不是素数 */
    }
    if (n % 2 == 0) {
        return n == 2;              /* 2 是唯一的偶素数 */
    }
    /* 只需要试除到 sqrt(n): 若 n = a*b 且 a<=b, 则 a <= sqrt(n) */
    for (int d = 3; (long long)d * d <= n; d += 2) {
        if (n % d == 0) {
            return 0;
        }
    }
    return 1;
}

/* 埃拉托斯特尼筛: 一次性找出所有素数, 比逐个判断快得多 */
static int sieve_primes(int limit, int *primes, int max_primes)
{
    /* 需要 calloc/free, 这里简单起见用固定大小数组 */
    static unsigned char composite[2001];
    int count = 0;

    if (limit > 2000 || limit < 2) {
        return 0;
    }
    memset(composite, 0, sizeof(composite));

    for (int i = 2; i <= limit; i++) {
        if (composite[i]) {
            continue;               /* 已经被更小的素数筛掉了 */
        }
        if (count < max_primes) {
            primes[count++] = i;
        }
        /* 从 i*i 开始筛, 更小的倍数早就被更小的素数筛过了 */
        for (long long j = (long long)i * i; j <= limit; j += i) {
            composite[j] = 1;
        }
    }
    return count;
}

static void puzzle_primes(void)
{
    puts("\n========== 2. 素数 ==========");

    printf("  1..100 中的素数: ");
    for (int i = 1; i <= 100; i++) {
        if (is_prime(i)) {
            printf("%d ", i);
        }
    }
    putchar('\n');

    printf("  判断特例: is_prime(1)=%d, is_prime(2)=%d, is_prime(97)=%d, is_prime(100)=%d\n",
           is_prime(1), is_prime(2), is_prime(97), is_prime(100));

    /* 筛法: 找出 200 以内的全部素数 */
    int primes[64];
    int n = sieve_primes(200, primes, 64);
    printf("  200 以内共有 %d 个素数, 前 20 个: ", n);
    for (int i = 0; i < n && i < 20; i++) {
        printf("%d ", primes[i]);
    }
    putchar('\n');
}

/* ==================== 3. 水仙花数 ==================== */
static void puzzle_narcissus(void)
{
    puts("\n========== 3. 水仙花数(3 位数, 各位立方和 = 自身) ==========");

    for (int n = 100; n <= 999; n++) {
        int hundreds = n / 100;
        int tens     = n / 10 % 10;
        int ones     = n % 10;
        if (hundreds * hundreds * hundreds +
            tens * tens * tens +
            ones * ones * ones == n) {
            printf("  %d\n", n);
        }
    }
    puts("  答案: 153, 370, 371, 407");
}

/* ==================== 4. 回文 ==================== */
static int is_palindrome_number(int n)
{
    if (n < 0) {
        return 0;
    }
    int rev = 0, x = n;
    while (x > 0) {
        rev = rev * 10 + x % 10;    /* 逐位取出拼成反序数 */
        x /= 10;
    }
    return rev == n;
}

static int is_palindrome_str(const char *s)
{
    size_t i = 0, j = strlen(s);
    if (j == 0) {
        return 1;
    }
    j--;                            /* j 指向最后一个字符 */
    while (i < j) {
        if (s[i] != s[j]) {
            return 0;
        }
        i++;
        j--;
    }
    return 1;
}

static void puzzle_palindrome(void)
{
    puts("\n========== 4. 回文 ==========");

    printf("  回文数: ");
    for (int n = 1; n <= 200; n++) {
        if (is_palindrome_number(n)) {
            printf("%d ", n);
        }
    }
    putchar('\n');

    const char *words[] = { "level", "hello", "abcba", "" };
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        printf("  \"%s\" -> %s\n", words[i], is_palindrome_str(words[i]) ? "是回文" : "不是");
    }
    puts("  (空串按定义为回文)");

    /* 中文陷阱: 上面的算法比较的是“字节”, 而一个汉字在 UTF-8 里占 3 个字节,
     * 所以 "上海自来水来自海上" 按字节首尾对比会失败。
     * 要正确处理必须按“字符”切分(用 mbstowcs 转成 wchar_t 再比较)。 */
    const char *cn = "上海自来水来自海上";
    size_t cn_len = strlen(cn);
    int byte_ok = 1;
    for (size_t i = 0, j = cn_len - 1; i < j; i++, j--) {
        if (cn[i] != cn[j]) { byte_ok = 0; break; }
    }
    printf("  \"%s\" 按字节比较: %s (%zu 字节)\n", cn, byte_ok ? "是" : "不是", cn_len);
    puts("  按字节比较对 UTF-8 中文是无效的: 每个汉字 3 字节, 首尾字节本来就不对应。");
    puts("  需要按字符处理时用 wchar_t + mbstowcs, 或者用专门的 UTF-8 库。");
}

/* ==================== 5. 进制转换 ==================== */
static void to_base(unsigned int n, int base, char *out, size_t out_size)
{
    static const char digits[] = "0123456789ABCDEF";
    char tmp[64];
    int  len = 0;

    if (out_size == 0) {
        return;
    }
    if (n == 0) {
        tmp[len++] = '0';
    }
    /* 先拿到低位, 所以是反序的 */
    while (n > 0 && len < (int)sizeof(tmp)) {
        tmp[len++] = digits[n % (unsigned int)base];
        n /= (unsigned int)base;
    }
    /* 再倒过来 */
    size_t k = 0;
    for (int i = len - 1; i >= 0 && k + 1 < out_size; i--) {
        out[k++] = tmp[i];
    }
    out[k] = '\0';
}

static void puzzle_base_convert(void)
{
    puts("\n========== 5. 进制转换 ==========");

    unsigned int nums[] = { 0, 10, 255, 1000, 65535 };
    char buf[64];

    for (size_t i = 0; i < sizeof(nums) / sizeof(nums[0]); i++) {
        unsigned int v = nums[i];
        to_base(v, 2,  buf, sizeof(buf));
        char bin[64];
        snprintf(bin, sizeof(bin), "%s", buf);

        to_base(v, 8,  buf, sizeof(buf));
        char oct[64];
        snprintf(oct, sizeof(oct), "%s", buf);

        to_base(v, 16, buf, sizeof(buf));
        printf("  %5u -> bin=%-18s oct=%-7s hex=0x%s\n", v, bin, oct, buf);
    }
    puts("  也可以用 printf 直接输出: %o(八进制) %x(十六进制), 但没有二进制 %b。");
}

/* ==================== 6. 杨辉三角 ==================== */
static void puzzle_pascal(void)
{
    puts("\n========== 6. 杨辉三角 ==========");

    const int rows = 8;
    int tri[8][8] = {0};

    for (int i = 0; i < rows; i++) {
        tri[i][0] = 1;                          /* 每行第一个是 1 */
        for (int j = 1; j <= i; j++) {
            tri[i][j] = tri[i - 1][j - 1] + tri[i - 1][j];   /* 肩上两数之和 */
        }
    }

    for (int i = 0; i < rows; i++) {
        for (int k = 0; k < rows - i - 1; k++) {
            printf("   ");                      /* 前面的空格让三角形居中 */
        }
        for (int j = 0; j <= i; j++) {
            printf("%-6d", tri[i][j]);
        }
        putchar('\n');
    }
}

/* ==================== 7. 最大公约数 / 最小公倍数 ==================== */
static int gcd(int a, int b)
{
    return b == 0 ? a : gcd(b, a % b);
}

static void puzzle_gcd_lcm(void)
{
    puts("\n========== 7. 最大公约数与最小公倍数 ==========");

    int pairs[][2] = { {12, 18}, {48, 60}, {1071, 462}, {17, 5} };
    for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
        int a = pairs[i][0], b = pairs[i][1];
        int g = gcd(a, b);
        int l = a / g * b;              /* 先除后乘, 避免溢出 */
        printf("  a=%-5d b=%-5d gcd=%-4d lcm=%d\n", a, b, g, l);
    }
}

/* ==================== 8. 斐波那契 ==================== */
static void puzzle_fibonacci(void)
{
    puts("\n========== 8. 斐波那契数列 ==========");

    int n = 20;
    unsigned long long a = 0, b = 1;
    printf("  前 %d 项: ", n);
    for (int i = 0; i < n; i++) {
        printf("%llu ", a);
        unsigned long long next = a + b;
        a = b;
        b = next;
    }
    putchar('\n');

    /* 经典变体: 台阶问题 */
    puts("  变体: 一次可以走 1 级或 2 级台阶, 走 n 级有多少种走法?");
    printf("  n=1..10 的走法数: ");
    a = 0; b = 1;
    for (int i = 0; i < 12; i++) {
        unsigned long long next = a + b;
        a = b;
        b = next;
        if (i >= 1 && i <= 10) {
            printf("%llu ", a);
        }
    }
    putchar('\n');
}

/* ==================== 9. 质因数分解 ==================== */
static void puzzle_factorize(long long n)
{
    printf("  %lld = ", n);

    int first = 1;
    for (long long p = 2; p * p <= n; p++) {
        while (n % p == 0) {
            printf("%s%lld", first ? "" : " * ", p);
            first = 0;
            n /= p;
        }
    }
    if (n > 1) {                        /* 剩下的一定是素数 */
        printf("%s%lld", first ? "" : " * ", n);
    }
    putchar('\n');
}

/* ==================== 10. 完数 ==================== */
static void puzzle_perfect_numbers(void)
{
    puts("\n========== 10. 完数(等于所有真因子之和) ==========");

    for (int n = 2; n <= 10000; n++) {
        int sum = 0;
        for (int d = 1; d <= n / 2; d++) {      /* 真因子最大不超过 n/2 */
            if (n % d == 0) {
                sum += d;
            }
        }
        if (sum == n) {
            printf("  %d = 1", n);
            for (int d = 2; d <= n / 2; d++) {
                if (n % d == 0) {
                    printf(" + %d", d);
                }
            }
            putchar('\n');
        }
    }
    puts("  10000 以内有: 6, 28, 496, 8128");
}

/* ==================== 11. 统计字符类型 ==================== */
static void puzzle_count_chars(void)
{
    puts("\n========== 11. 统计各类字符个数 ==========");

    const char *s = "Hello, World! 2024年12月 C语言 3.14";
    int upper = 0, lower = 0, digit = 0, space = 0, other = 0;

    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        if (*p >= 'A' && *p <= 'Z')      upper++;
        else if (*p >= 'a' && *p <= 'z') lower++;
        else if (*p >= '0' && *p <= '9') digit++;
        else if (*p == ' ')              space++;
        else                             other++;   /* 含中文的 UTF-8 字节、标点 */
    }

    printf("  字符串: %s\n", s);
    printf("  大写=%d 小写=%d 数字=%d 空格=%d 其它=%d\n",
           upper, lower, digit, space, other);
    puts("  注意: 中文在 UTF-8 里占 3 个字节, 会统计成 3 个 other。");
}

/* ==================== 12. 数组统计与简单查询 ==================== */
static void puzzle_array_stats(void)
{
    puts("\n========== 12. 数组统计 ==========");

    int a[] = {88, 75, 92, 63, 55, 78, 90, 45, 82, 71};
    size_t n = sizeof(a) / sizeof(a[0]);

    int sum = 0, max = a[0], min = a[0];
    int pass = 0, fail = 0;

    for (size_t i = 0; i < n; i++) {
        sum += a[i];
        if (a[i] > max) max = a[i];
        if (a[i] < min) min = a[i];
        if (a[i] >= 60)  pass++;
        else             fail++;
    }

    printf("  数据: ");
    for (size_t i = 0; i < n; i++) printf("%d ", a[i]);
    printf("\n  个数=%zu 总分=%d 平均=%.2f 最高=%d 最低=%d\n",
           n, sum, (double)sum / (double)n, max, min);
    printf("  及格 %d 人, 不及格 %d 人\n", pass, fail);

    /* 找出所有超过平均分的人 */
    double avg = (double)sum / (double)n;
    printf("  超过平均分的分数: ");
    for (size_t i = 0; i < n; i++) {
        if ((double)a[i] > avg) {
            printf("%d ", a[i]);
        }
    }
    putchar('\n');

    /* 选择排序的变体: 求第二名 */
    int first = a[0], second = -1;
    for (size_t i = 1; i < n; i++) {
        if (a[i] > first) {
            second = first;
            first = a[i];
        } else if (a[i] > second && a[i] != first) {
            second = a[i];
        }
    }
    printf("  最高分 %d, 第二高 %d\n", first, second);
}

/* ==================== 13. 三角形 / 金字塔 ==================== */
static void puzzle_pyramid(void)
{
    puts("\n========== 13. 空心菱形 ==========");

    const int n = 4;
    for (int i = -n; i <= n; i++) {
        int stars = 2 * (n - (i < 0 ? -i : i)) + 1;
        int spaces = n - (stars - 1) / 2;

        for (int k = 0; k < spaces; k++) putchar(' ');
        for (int k = 0; k < stars; k++) {
            /* 只在两端打印星号, 中间留空 => 空心 */
            if (k == 0 || k == stars - 1) {
                putchar('*');
            } else {
                putchar(' ');
            }
        }
        putchar('\n');
    }
}

int main(void)
{
    puzzle_99_table();
    puzzle_primes();
    puzzle_narcissus();
    puzzle_palindrome();
    puzzle_base_convert();
    puzzle_pascal();
    puzzle_gcd_lcm();
    puzzle_fibonacci();

    puts("\n========== 9. 质因数分解 ==========");
    long long nums[] = { 12, 100, 97, 1024, 123456, 999999 };
    for (size_t i = 0; i < sizeof(nums) / sizeof(nums[0]); i++) {
        puzzle_factorize(nums[i]);
    }

    puzzle_perfect_numbers();
    puzzle_count_chars();
    puzzle_array_stats();
    puzzle_pyramid();
    return 0;
}
