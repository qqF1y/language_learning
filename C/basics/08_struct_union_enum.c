/*
 * 08_struct_union_enum.c —— 结构体 / 枚举 / 联合体 / 位域
 *
 * 有了 struct, C 才勉强能"描述现实世界的东西"。
 * 记住: struct 是"把几个变量打包成一种新类型", 赋值时是逐字节拷贝。
 */
#include <stdio.h>
#include <string.h>
#include <stddef.h>     /* offsetof */

/* ==================== 结构体 ==================== */
struct Point {
    int x;
    int y;
};

/* typedef 起别名, 之后就不用每次写 struct 了 */
typedef struct {
    char name[32];
    int  score;
} Student;

/* 结构体嵌套 */
typedef struct {
    char    title[32];
    Student students[3];
    int     count;
} ClassRoom;

/* ==================== 枚举 ==================== */
typedef enum {
    COLOR_RED = 0,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_COUNT         /* 常用小技巧: 再放一个"总数", 方便遍历和做边界检查 */
} Color;

typedef enum {
    STATUS_OK = 200,
    STATUS_NOT_FOUND = 404,
    STATUS_ERROR = 500,
} HttpStatus;

/* ==================== 联合体: 所有成员共用同一块内存 ==================== */
typedef union {
    unsigned int  i;
    float         f;
    unsigned char bytes[4];
} Number;

/* ==================== 位域: 精确控制用几个 bit ==================== */
typedef struct {
    unsigned int a    : 1;      /* 占 1 位, 只能存 0/1 */
    unsigned int b    : 1;
    unsigned int c    : 1;
    unsigned int      : 5;      /* 无名位域: 占位跳过 5 位(常用于对齐) */
    unsigned int mode : 3;      /* 占 3 位, 只能存 0..7 */
} Flags;

/* ==================== 工具函数 ==================== */
static const char *color_name(Color c)
{
    static const char *names[COLOR_COUNT] = { "红", "绿", "蓝" };
    return c < COLOR_COUNT ? names[c] : "未知";
}

static void print_student(const Student *s)     /* 传指针: 不拷贝整个结构体 */
{
    printf("    %-10s %3d\n", s->name, s->score);   /* -> 等价于 (*s).name */
}

static void print_class(const ClassRoom *cls)
{
    printf("  班级「%s」共 %d 人:\n", cls->title, cls->count);
    for (int i = 0; i < cls->count; i++) {
        print_student(&cls->students[i]);
    }
}

/* 传结构体本身: 会发生整份拷贝, 大了就有性能代价 */
static void bump_by_value(Student s)
{
    printf("    函数内部拿到的是副本, 当前 score=%d\n", s.score);
    s.score = 100;                  /* 改的是副本, 外面看不到 */
}

/* 传结构体指针: 高效, 也能修改原数据 */
static void bump_by_pointer(Student *s)
{
    s->score = 100;                 /* 改的是本体 */
}

static int is_little_endian(void)
{
    unsigned int x = 1;
    return *(unsigned char *)&x == 1;   /* 低地址存的是最低字节 => 小端 */
}

int main(void)
{
    /* ==================== 1. 结构体基本用法 ==================== */
    puts("========== 1. 结构体 ==========");

    struct Point p1 = {3, 4};               /* 顺序初始化 */
    struct Point p2 = {.y = 20, .x = 10};   /* 指定初始化(C99), 顺序随意 */
    struct Point p3 = p1;                   /* 整体拷贝 */

    printf("  p1 = (%d, %d)\n", p1.x, p1.y);
    printf("  p2 = (%d, %d)\n", p2.x, p2.y);

    p3.x = 99;
    printf("  p3 改成 (%d, %d), p1 仍是 (%d, %d)  <-- 结构体赋值是拷贝, 不是共享\n",
           p3.x, p3.y, p1.x, p1.y);

    p3 = p2;                                /* 也支持整体赋值 */
    printf("  p3 = p2 之后 p3 = (%d, %d)\n", p3.x, p3.y);

    puts("\n  内存布局:");
    printf("    sizeof(struct Point) = %zu\n", sizeof(struct Point));
    printf("    offsetof(x) = %zu, offsetof(y) = %zu\n",
           offsetof(struct Point, x), offsetof(struct Point, y));
    puts("    (结构体可能有“填充字节”以满足对齐要求, 所以 sizeof 不一定等于成员之和)");

    /* ==================== 2. 结构体数组 ==================== */
    puts("\n========== 2. 结构体数组 ==========");

    Student all[3] = {
        {"Tom",   91},
        {"Jerry", 78},
        {"Spike", 85},
    };
    size_t n = sizeof(all) / sizeof(all[0]);

    puts("  原始顺序:");
    for (size_t i = 0; i < n; i++) {
        print_student(&all[i]);
    }

    /* 按分数从高到低选择排序 —— 交换的是整个结构体 */
    for (size_t i = 0; i + 1 < n; i++) {
        size_t best = i;
        for (size_t j = i + 1; j < n; j++) {
            if (all[j].score > all[best].score) {
                best = j;
            }
        }
        if (best != i) {
            Student t = all[i];
            all[i] = all[best];
            all[best] = t;
        }
    }

    puts("  按分数排序后:");
    for (size_t i = 0; i < n; i++) {
        print_student(&all[i]);
    }

    /* 查找最高分 */
    const Student *top = &all[0];
    for (size_t i = 1; i < n; i++) {
        if (all[i].score > top->score) {
            top = &all[i];
        }
    }
    printf("  最高分: %s (%d)\n", top->name, top->score);

    /* ==================== 3. 结构体指针与传参 ==================== */
    puts("\n========== 3. 传结构体 vs 传指针 ==========");

    Student s = {"Alice", 60};
    printf("  初始: %s %d\n", s.name, s.score);

    bump_by_value(s);
    printf("  bump_by_value 之后: %d  (没变, 改的是副本)\n", s.score);

    bump_by_pointer(&s);
    printf("  bump_by_pointer 之后: %d (变了)\n", s.score);

    /*** 结构体内部有指针时的“浅拷贝”陷阱 ***/
    puts("\n  注意: 结构体里的数组成员会被拷贝, 但指针成员只是拷贝地址 ——");
    puts("        这就是“浅拷贝”。需要深拷贝时必须手动再 malloc + memcpy。");

    /* ==================== 4. 结构体嵌套 ==================== */
    puts("\n========== 4. 结构体嵌套 ==========");

    ClassRoom cls;
    snprintf(cls.title, sizeof(cls.title), "三年二班");
    cls.count = 3;
    snprintf(cls.students[0].name, sizeof(cls.students[0].name), "Han");
    cls.students[0].score = 88;
    snprintf(cls.students[1].name, sizeof(cls.students[1].name), "Luke");
    cls.students[1].score = 92;
    snprintf(cls.students[2].name, sizeof(cls.students[2].name), "Leia");
    cls.students[2].score = 95;

    print_class(&cls);

    /* ==================== 5. 枚举 ==================== */
    puts("\n========== 5. 枚举 ==========");

    Color c = COLOR_GREEN;
    printf("  COLOR_RED=%d COLOR_GREEN=%d COLOR_BLUE=%d\n",
           COLOR_RED, COLOR_GREEN, COLOR_BLUE);
    printf("  c = %d -> %s\n", c, color_name(c));

    for (Color i = COLOR_RED; i < COLOR_COUNT; i++) {
        printf("  遍历枚举: %s\n", color_name(i));
    }

    HttpStatus st = STATUS_NOT_FOUND;
    switch (st) {
    case STATUS_OK:
        puts("  200 OK");
        break;
    case STATUS_NOT_FOUND:
        puts("  404 Not Found");
        break;
    case STATUS_ERROR:
        puts("  500 Server Error");
        break;
    }
    puts("  枚举本质就是整数常量, 好处是名字自解释、switch 时可读性好");

    /* ==================== 6. 联合体 ==================== */
    puts("\n========== 6. 联合体 union ==========");

    Number num;
    num.i = 0x41424344u;
    printf("  num.i = 0x%X\n", num.i);
    printf("  按 4 个字节看: %02X %02X %02X %02X\n",
           num.bytes[0], num.bytes[1], num.bytes[2], num.bytes[3]);
    puts("  低地址放的是 0x44 => 小端(little-endian)。大端机器上顺序会反。");

    num.f = 1.0f;
    printf("  写 float 1.0f 后, 按 int 读 = 0x%X (IEEE-754 规定就是 0x3F800000)\n", num.i);

    printf("  sizeof(union Number) = %zu  <-- 等于最大成员的大小, 成员互相覆盖\n",
           sizeof(num));
    printf("  本机字节序: %s\n", is_little_endian() ? "小端" : "大端");

    /* ==================== 7. 位域 ==================== */
    puts("\n========== 7. 位域 ==========");

    Flags f = {0};
    f.a = 1;
    f.b = 0;
    f.mode = 5;

    printf("  a=%u b=%u c=%u mode=%u, sizeof(Flags)=%zu 字节\n",
           f.a, f.b, f.c, f.mode, sizeof(f));
    puts("  用一个字节以上的空间存了 5 个字段, 省内存(协议解析、寄存器操作常用)");

    unsigned int big = 9;
    f.mode = big;                   /* 3 位最多存 7 */
    printf("  mode 赋值 9 之后 = %u  <-- 高位被丢掉(9 的二进制 1001, 只留 001)\n",
           f.mode);
    return 0;
}
