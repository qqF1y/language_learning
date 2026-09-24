/*
 * 12_bit_operations.c —— 位运算
 *
 * 位运算直接操作二进制, 是嵌入式、协议解析、压缩、加密、性能优化里的基本功。
 *
 *   &   按位与   有 0 则 0          常用于"取某几位"、"清位"
 *   |   按位或   有 1 则 1          常用于"置位"、"合并字段"
 *   ^   按位异或 相同为 0 不同为 1   常用于"翻转位"、"不借助临时变量交换"
 *   ~   按位取反 0/1 互换
 *   <<  左移     右边补 0, 相当于乘 2^n
 *   >>  右移     左边补符号位/0, 相当于除以 2^n (只对无符号是确定的)
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* 打印 32 位二进制, 每 8 位空一格, 便于观察 */
static void print_binary(unsigned int x)
{
    for (int i = 31; i >= 0; i--) {
        putchar(((x >> i) & 1u) ? '1' : '0');
        if (i % 8 == 0 && i != 0) {
            putchar(' ');
        }
    }
}

#define BIT(n)          (1u << (n))         /* 第 n 位为 1 的掩码 */
#define SET_BIT(x, n)   ((x) |= BIT(n))     /* 置位 */
#define CLEAR_BIT(x, n) ((x) &= ~BIT(n))    /* 清位 */
#define FLIP_BIT(x, n)  ((x) ^= BIT(n))     /* 翻转 */
#define GET_BIT(x, n)   (((x) >> (n)) & 1u) /* 取位 */

/* ==================== 1. 六个运算符 ==================== */
static void demo_operators(void)
{
    puts("========== 1. 六个位运算符 ==========");

    unsigned int a = 0x0Fu;                 /* 00000000 00000000 00000000 00001111 */
    unsigned int b = 0x33u;                 /* 00000000 00000000 00000000 00110011 */

    printf("a = 0x%08X = ", a); print_binary(a); putchar('\n');
    printf("b = 0x%08X = ", b); print_binary(b); putchar('\n');
    putchar('\n');

    printf("a & b  = 0x%08X = ", a & b);  print_binary(a & b);  puts("  (按位与: 都为 1 才是 1)");
    printf("a | b  = 0x%08X = ", a | b);  print_binary(a | b);  puts("  (按位或: 有一个 1 就是 1)");
    printf("a ^ b  = 0x%08X = ", a ^ b);  print_binary(a ^ b);  puts("  (异或: 不同为 1)");
    printf("~a     = 0x%08X = ", ~a);     print_binary(~a);     puts("  (取反)");
    printf("a << 4 = 0x%08X = ", a << 4); print_binary(a << 4); puts("  (左移 4 位 = 乘 16)");
    printf("b >> 2 = 0x%08X = ", b >> 2); print_binary(b >> 2); puts("  (右移 2 位 = 除 4)");
}

/* ==================== 2. 置位 / 清位 / 取位 / 翻转 ==================== */
static void demo_bit_flags(void)
{
    puts("\n========== 2. 位的四种基本操作(开关标志的经典用法) ==========");

    unsigned int flags = 0;
    const unsigned int LED = 3;             /* 用第 3 位代表某个开关 */

    printf("初始        flags = 0x%02X\n", flags);

    SET_BIT(flags, LED);
    printf("置位第 %u 位  flags = 0x%02X, 该位 = %u\n", LED, flags, GET_BIT(flags, LED));

    CLEAR_BIT(flags, LED);
    printf("清位第 %u 位  flags = 0x%02X, 该位 = %u\n", LED, flags, GET_BIT(flags, LED));

    FLIP_BIT(flags, LED);
    printf("翻转第 %u 位  flags = 0x%02X, 该位 = %u\n", LED, flags, GET_BIT(flags, LED));

    FLIP_BIT(flags, LED);
    printf("再翻转一次  flags = 0x%02X (回到原样)\n", flags);

    /* 一次操作多个位 */
    unsigned int mode = 0;
    mode |= (BIT(0) | BIT(2) | BIT(4));     /* 同时置三个位 */
    printf("\nmode = 0x%02X = ", mode); print_binary(mode); putchar('\n');

    puts("嵌入式里每个 bit 表示一个功能开关, 这样最省内存也最快。");
}

/* ==================== 3. 经典位技巧 ==================== */
static int is_even(unsigned int x)
{
    return (x & 1u) == 0;                   /* x % 2 的位运算写法 */
}

static int is_power_of_two(unsigned int x)
{
    /* 2 的幂的二进制只有一个 1, 减 1 之后变成后面全是 1, 相与必为 0 */
    return x != 0 && (x & (x - 1u)) == 0;
}

static int count_ones(unsigned int x)
{
    /* Brian Kernighan 算法: x &= x-1 每次干掉最右边的那个 1
     * 循环次数 = 1 的个数, 比逐位检查快 */
    int n = 0;
    while (x) {
        x &= x - 1u;
        n++;
    }
    return n;
}

static unsigned int lowest_set_bit(unsigned int x)
{
    return x & (~x + 1u);                   /* 只保留最低位的 1, 也叫 lowbit */
}

static void swap_no_temp(unsigned int *a, unsigned int *b)
{
    if (a == b) {                           /* 同一个变量时异或会把值清成 0 */
        return;
    }
    *a ^= *b;
    *b ^= *a;
    *a ^= *b;
}

static void demo_tricks(void)
{
    puts("\n========== 3. 经典位技巧 ==========");

    printf("is_even(7) = %d, is_even(8) = %d\n", is_even(7), is_even(8));

    unsigned int pows[] = {1, 2, 3, 4, 8, 12, 16, 1024, 1025};
    printf("判断 2 的幂: ");
    for (size_t i = 0; i < sizeof(pows) / sizeof(pows[0]); i++) {
        printf("%u:%s ", pows[i], is_power_of_two(pows[i]) ? "是" : "否");
    }
    putchar('\n');

    unsigned int x = 0xF0F0F0F0u;
    printf("popcount(0x%08X) = %d\n", x, count_ones(x));
    printf("popcount(0x%08X) = %d\n", 0xFFFFFFFFu, count_ones(0xFFFFFFFFu));

    printf("lowbit(0b1100 = 12) = %u\n", lowest_set_bit(12u));   /* 12 = 1100 -> 4 */
    printf("lowbit(0b1010 = 10) = %u\n", lowest_set_bit(10u));   /* 10 = 1010 -> 2 */

    unsigned int m = 0xAAAAu, n = 0x5555u;
    printf("\n交换前: m=0x%X n=0x%X\n", m, n);
    swap_no_temp(&m, &n);
    printf("异或交换后: m=0x%X n=0x%X\n", m, n);
    puts("(实际项目别这么写, 可读性差; 用临时变量编译器会优化得一样快)");
}

/* ==================== 4. 位图(bitmap) ==================== */
static void demo_bitmap(void)
{
    puts("\n========== 4. 位图: 1 个 bit 表示一个布尔值 ==========");

    /* 用一个 32 位整数表示 "0..31 这些数字有没有出现过" */
    uint32_t bits = 0;
    const int nums[] = {3, 7, 15, 31, 7, 3, 15, 22};
    size_t n = sizeof(nums) / sizeof(nums[0]);

    for (size_t i = 0; i < n; i++) {
        bits |= 1u << nums[i];              /* 标记出现过 */
    }

    printf("原始数据: ");
    for (size_t i = 0; i < n; i++) {
        printf("%d ", nums[i]);
    }
    putchar('\n');

    printf("去重后(%d 个): ", count_ones(bits));
    for (int i = 0; i < 32; i++) {
        if ((bits >> i) & 1u) {
            printf("%d ", i);
        }
    }
    printf("\nbits = 0x%08X, 只占 %zu 字节(用 bool 数组要 %zu 字节)\n",
           bits, sizeof(bits), 32 * sizeof(char));

    puts("位图是 Bloom filter、位集合、内存分配器、Linux 文件系统里都在用的基础结构。");
}

/* ==================== 5. 实际应用: 拆包/打包颜色 ==================== */
static void demo_pack_unpack(void)
{
    puts("\n========== 5. 实际应用: 拆分与合并字段 ==========");

    /* 32 位里塞三个 8 位字段: 0x00RRGGBB */
    uint32_t color = 0x00FF8040u;

    unsigned int r = (color >> 16) & 0xFFu;     /* 取 bit16..23 */
    unsigned int g = (color >>  8) & 0xFFu;     /* 取 bit8..15  */
    unsigned int b =  color        & 0xFFu;     /* 取 bit0..7   */

    printf("color = 0x%06X -> R=%u(0x%02X) G=%u(0x%02X) B=%u(0x%02X)\n",
           color, r, r, g, g, b, b);

    /* 反过来打包 */
    uint32_t packed = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    printf("重新打包 = 0x%06X (一致: %s)\n", packed, packed == color ? "是" : "否");

    puts("\n这是解析协议头、图像像素、寄存器值的通用套路:");
    puts("  取值: (word >> offset) & mask");
    puts("  写值: word = (word & ~(mask << offset)) | ((value & mask) << offset)");
}

/* ==================== 6. 查看内存字节 ==================== */
static void hex_dump(const void *data, size_t len)
{
    const unsigned char *p = data;
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", p[i]);
        if ((i + 1) % 8 == 0) {
            putchar('\n');
        }
    }
    if (len % 8 != 0) {
        putchar('\n');
    }
}

static void demo_hex_dump(void)
{
    puts("\n========== 6. hex dump: 看清内存里的字节 ==========");

    uint32_t v = 0x01020304u;
    printf("uint32_t v = 0x%08X\n", v);
    printf("内存里的字节顺序: ");
    hex_dump(&v, sizeof(v));
    puts("低地址是 04 -> 小端(little-endian)");

    const char *s = "ABC";
    printf("\n字符串 \"ABC\" 的内存(含结尾的 00): ");
    hex_dump(s, 4);

    int arr[3] = {0x11223344, 0x55667788, 0x99AABBCC};
    printf("\nint[3] 的内存:\n");
    hex_dump(arr, sizeof(arr));
    puts("(可以清楚看到每个 int 的 4 个字节是怎么排的)");

    unsigned int endian_test = 1;
    if (*(unsigned char *)&endian_test == 1) {
        puts("\n本机字节序: 小端 —— 多字节数据的低位在低地址");
    } else {
        puts("\n本机字节序: 大端");
    }
    puts("网络传输统一用大端(网络字节序), 所以要用 htons/htonl 转换。");
}

int main(void)
{
    demo_operators();
    demo_bit_flags();
    demo_tricks();
    demo_bitmap();
    demo_pack_unpack();
    demo_hex_dump();
    return 0;
}
