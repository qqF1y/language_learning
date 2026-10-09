/*
 * 04_arrays_strings.c —— 数组与字符串
 *
 * 核心事实: C 没有真正的"字符串类型", 字符串就是"以 '\0' 结尾的 char 数组"。
 * 数组中所有元素在内存里是连续存放的, 这是后面理解指针的关键。
 */
#include <stdio.h>
#include <string.h>     /* strlen, strcmp, strchr, strstr, strtok ... */
#include <stdlib.h>     /* atoi, strtol */
#include <ctype.h>      /* toupper, tolower */

/* ==================== 一维数组 ==================== */
static void demo_array(void)
{
    puts("========== 1. 一维数组 ==========");

    int squares[6];                       /* 局部数组不初始化时内容是垃圾值 */
    int b[5]  = {1, 2, 3, 4, 5};
    int c[5]  = {1, 2};                   /* 其余元素自动补 0 -> {1,2,0,0,0} */
    int d[]   = {10, 20, 30};             /* 长度由初始化列表决定 -> 3 */

    for (int i = 0; i < 6; i++) {
        squares[i] = i * i;               /* 先赋值再使用, 不要读垃圾值 */
    }
    printf("squares = { ");
    for (size_t i = 0; i < sizeof(squares) / sizeof(squares[0]); i++) {
        printf("%d ", squares[i]);
    }
    printf("}\n");

    /* 求数组长度的惯用式子: 总字节数 / 单个元素字节数 */
    printf("sizeof(b) = %zu 字节, 元素个数 = %zu\n",
           sizeof(b), sizeof(b) / sizeof(b[0]));
    printf("sizeof(d)/sizeof(d[0]) = %zu\n", sizeof(d) / sizeof(d[0]));

    printf("b  = { ");
    for (size_t i = 0; i < sizeof(b) / sizeof(b[0]); i++) {
        printf("%d ", b[i]);
    }
    printf("}\n");

    printf("c  = { %d %d %d %d %d }   <-- 没写出来的元素被补成 0\n",
           c[0], c[1], c[2], c[3], c[4]);

    /* 经典三件套: 求和、求最值、逆序 */
    int sum = 0, max = b[0], min = b[0];
    for (size_t i = 0; i < 5; i++) {
        sum += b[i];
        if (b[i] > max) max = b[i];
        if (b[i] < min) min = b[i];
    }
    printf("b: 和=%d 最大=%d 最小=%d 平均=%.2f\n", sum, max, min, (double)sum / 5);

    printf("倒着打印 b: ");
    for (int i = 4; i >= 0; i--) {
        printf("%d ", b[i]);
    }
    putchar('\n');

    /* 原地反转: 首尾两个下标往中间凑 */
    for (int i = 0, j = 4; i < j; i++, j--) {
        int t = b[i];
        b[i] = b[j];
        b[j] = t;
    }
    printf("原地反转后 b = { %d %d %d %d %d }\n", b[0], b[1], b[2], b[3], b[4]);

    puts("警告: a[10] 越界写不会报错, 会悄悄踩坏别的变量 —— 这就是缓冲区溢出。");
}

/* ==================== 二维数组 ==================== */
static void demo_2d_array(void)
{
    puts("\n========== 2. 二维数组 ==========");

    int m[3][4] = {
        { 1,  2,  3,  4},
        { 5,  6,  7,  8},
        { 9, 10, 11, 12},
    };

    printf("总字节 %zu, 行数 %zu, 列数 %zu\n",
           sizeof(m), sizeof(m) / sizeof(m[0]), sizeof(m[0]) / sizeof(m[0][0]));
    puts("注意: 内存里是“按行连续”存放的, m[0][4] 其实就是 m[1][0]");

    puts("按行打印:");
    for (size_t i = 0; i < 3; i++) {
        printf("  ");
        for (size_t j = 0; j < 4; j++) {
            printf("%3d ", m[i][j]);
        }
        putchar('\n');
    }

    /* 转置: t[j][i] = m[i][j] */
    int t[4][3];
    for (size_t i = 0; i < 3; i++) {
        for (size_t j = 0; j < 4; j++) {
            t[j][i] = m[i][j];
        }
    }

    puts("转置后 (4 行 3 列):");
    for (size_t i = 0; i < 4; i++) {
        printf("  ");
        for (size_t j = 0; j < 3; j++) {
            printf("%3d ", t[i][j]);
        }
        putchar('\n');
    }
}

/* ==================== 字符串 ==================== */
static void demo_string_basic(void)
{
    puts("\n========== 3. 字符串 ==========");

    char        s1[] = "hello";     /* 数组: 6 字节, 末尾有 '\0', 内容可改 */
    char        s2[16] = "abc";     /* 剩余字节补 0 */
    const char *s3 = "world";       /* 指针: 指向只读的字符串字面量 */

    printf("sizeof(s1) = %zu  <-- 含结尾的 '\\0'\n", sizeof(s1));
    printf("strlen(s1) = %zu  <-- 不含 '\\0'\n", strlen(s1));
    printf("s1=%s s2=%s s3=%s\n", s1, s2, s3);

    s1[0] = 'H';                    /* 数组可以改: "Hello" */
    /* s3[0] = 'W';                   错误! 字面量是只读的, 会段错误 */

    /* 逐字符遍历: 靠 '\0' 判断结束 */
    printf("逐字符打印 s3: ");
    for (const char *p = s3; *p != '\0'; p++) {
        putchar(*p);
    }
    putchar('\n');

    /* 手动实现的 strlen / strcpy, 理解库函数到底在做什么 */
    size_t len = 0;
    while (s1[len] != '\0') {
        len++;
    }
    printf("手写 strlen(s1) = %zu (库函数给的是 %zu)\n", len, strlen(s1));

    char copy[32];
    size_t i = 0;
    while ((copy[i] = s1[i]) != '\0') {
        i++;
    }
    printf("手写 copy = %s\n", copy);
}

/* ==================== 常用字符串库函数 ==================== */
static void demo_string_lib(void)
{
    puts("\n========== 4. 常用字符串函数 ==========");

    /* 拼接用 snprintf 最安全: 它知道目标缓冲区有多大 */
    char buf[64];
    snprintf(buf, sizeof(buf), "%s, %s! 长度=%d", "hello", "world", 123);
    printf("snprintf 结果: \"%s\"\n", buf);

    /* 截断演示: 缓冲区小的时候 snprintf 会安全截断而不是溢出 */
    const char *long_src = "0123456789abcdef";
    char small[10];
    snprintf(small, sizeof(small), "%s", long_src);
    printf("缓冲区只有 10 字节 -> \"%s\" (被安全截断而不是溢出)\n", small);

    /* 比较: 返回 <0 / 0 / >0 */
    printf("strcmp(\"abc\",\"abc\") = %2d\n", strcmp("abc", "abc"));
    printf("strcmp(\"abc\",\"abd\") = %2d  <-- 负数表示前面小\n", strcmp("abc", "abd"));
    printf("strcmp(\"b\",\"a\")     = %2d  <-- 正数表示前面大\n", strcmp("b", "a"));
    puts("注意: 判相等必须用 strcmp(s1,s2)==0, 不能用 s1==s2(那比较的是地址)");

    /* 查找 */
    char text[] = "hello world, hello c";
    char *p = strchr(text, 'w');            /* 找字符 */
    printf("strchr(text,'w')  -> %s\n", p ? p : "(没找到)");

    char *q = strstr(text, "world");        /* 找子串 */
    printf("strstr(text,\"world\") -> %s\n", q ? q : "(没找到)");

    /* 分割: strtok 会修改原字符串(把分隔符换成 '\0'), 所以要用副本 */
    char csv[] = "apple,banana;cherry";
    int idx = 0;
    for (char *tok = strtok(csv, ",;"); tok != NULL; tok = strtok(NULL, ",;")) {
        printf("  第 %d 段: %s\n", ++idx, tok);
    }

    /* 字符串 <-> 数字 */
    const char *num = "1234";
    printf("atoi(\"%s\") = %d, 加 1 得 %d\n", num, atoi(num), atoi(num) + 1);

    const char *mixed = "42abc";
    char *end = NULL;
    long v = strtol(mixed, &end, 10);       /* 比 atoi 更能发现错误 */
    printf("strtol(\"%s\") = %ld, 停在 \"%s\"  <-- 用 end 判断有没有解析干净\n",
           mixed, v, end);

    /* 大小写转换 */
    char lower[] = "Hello C World";
    for (size_t i = 0; i < strlen(lower); i++) {
        lower[i] = (char)toupper((unsigned char)lower[i]);
    }
    printf("转大写: %s\n", lower);
}

/* ==================== 字符串经典处理 ==================== */
static void demo_string_algo(void)
{
    puts("\n========== 5. 字符串经典处理 ==========");

    /* 5.1 原地反转 */
    char s[] = "abcdefg";
    size_t n = strlen(s);
    for (size_t i = 0, j = n - 1; i < j; i++, j--) {
        char t = s[i];
        s[i] = s[j];
        s[j] = t;
    }
    printf("反转后: %s\n", s);

    /* 5.2 统计单词个数 */
    const char *sentence = "  the quick  brown fox jumps  ";
    int words = 0, in_word = 0;
    for (const char *p = sentence; *p != '\0'; p++) {
        if (isspace((unsigned char)*p)) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            words++;
        }
    }
    printf("\"%s\" 里有 %d 个单词\n", sentence, words);

    /* 5.3 统计各类字符 */
    const char *mixed = "Abc 123 !@#\tXYZ";
    int letters = 0, digits = 0, spaces = 0, others = 0;
    for (const char *p = mixed; *p != '\0'; p++) {
        unsigned char c = (unsigned char)*p;
        if (isalpha(c))       letters++;
        else if (isdigit(c))  digits++;
        else if (isspace(c))  spaces++;
        else                  others++;
    }
    printf("字母=%d 数字=%d 空白=%d 其它=%d\n", letters, digits, spaces, others);

    /* 5.4 判断回文 */
    const char *pal = "level";
    size_t i = 0, j = strlen(pal);
    int is_pal = 1;
    if (j > 0) {
        j--;
        while (i < j) {
            if (pal[i] != pal[j]) { is_pal = 0; break; }
            i++;
            j--;
        }
    }
    printf("\"%s\" 是回文吗? %s\n", pal, is_pal ? "是" : "不是");
}

int main(void)
{
    demo_array();
    demo_2d_array();
    demo_string_basic();
    demo_string_lib();
    demo_string_algo();
    return 0;
}
