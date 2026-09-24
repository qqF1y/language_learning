/*
 * 11_file_io.c —— 文件读写
 *
 * 三种常用姿势:
 *   1. 文本模式: fprintf/fscanf, fgets/fputs  —— 人类可读
 *   2. 二进制模式: fwrite/fread               —— 高效, 但换机器可能不兼容
 *   3. 定位: fseek/ftell/rewind               —— 想读哪就读哪
 *
 * 铁律: fopen 之后一定要 fclose; 每层 fopen 都要检查是否返回 NULL。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TXT_FILE "demo_11_text.txt"
#define BIN_FILE "demo_11_bin.dat"

typedef struct {
    int    id;
    char   name[16];
    double score;
} Record;

/* ==================== 1. 写文本文件 ==================== */
static void write_text(void)
{
    puts("========== 1. 写文本文件 ==========");

    /* "w": 文件不存在则创建, 存在则清空 */
    FILE *fp = fopen(TXT_FILE, "w");
    if (fp == NULL) {
        perror("fopen");                /* perror 会打印系统给的错误原因 */
        return;
    }

    fprintf(fp, "C 语言文件读写示例\n");                 /* 像 printf 一样, 只是写到文件 */
    fputs("第二行: hello file io\n", fp);                /* 只写字符串, 不带格式 */
    for (int i = 1; i <= 3; i++) {
        fprintf(fp, "第 %d 条数据: %d\n", i, i * 100);
    }

    if (fclose(fp) != 0) {              /* 一定要关! 否则缓冲区里的内容可能没落盘 */
        perror("fclose");
    }
    printf("  已写入 %s\n", TXT_FILE);
}

/* ==================== 2. 读文本文件 ==================== */
static void read_text(void)
{
    puts("\n========== 2. 读文本文件(fgets 逐行) ==========");

    FILE *fp = fopen(TXT_FILE, "r");
    if (fp == NULL) {
        perror("fopen");
        return;
    }

    char line[256];
    int  no = 0;

    /* fgets 最多读 sizeof(line)-1 个字符, 并在末尾补 '\0' —— 不会溢出 */
    while (fgets(line, sizeof(line), fp) != NULL) {
        printf("  %2d| %s", ++no, line);    /* line 里已经带了 '\n', 不用再补 */
    }

    if (feof(fp)) {
        printf("  读完了, 共 %d 行\n", no);
    }
    fclose(fp);

    puts("  为什么不用 gets()? 因为它不检查长度, 已被 C11 从标准中删除。");
}

/* ==================== 3. 追加内容 ==================== */
static void append_text(void)
{
    puts("\n========== 3. 追加模式 \"a\" ==========");

    FILE *fp = fopen(TXT_FILE, "a");    /* "a": 追加, 不会清空原内容 */
    if (fp == NULL) {
        perror("fopen");
        return;
    }
    fprintf(fp, "这是后来追加的一行\n");
    fclose(fp);

    /* 验证一下确实追加而不是覆盖 */
    fp = fopen(TXT_FILE, "r");
    if (fp == NULL) {
        return;
    }
    char line[256];
    int  total = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        total++;
    }
    fclose(fp);
    printf("  追加后共 %d 行(原来 5 行)\n", total);
}

/* ==================== 4. 逐字符统计(类似 wc) ==================== */
static void count_file(void)
{
    puts("\n========== 4. 逐字符统计 ==========");

    FILE *fp = fopen(TXT_FILE, "r");
    if (fp == NULL) {
        perror("fopen");
        return;
    }

    long chars = 0, lines = 0, words = 0;
    int  c;
    int  in_word = 0;

    /* fgetc 返回 int 而不是 char —— 因为它要用 EOF(-1) 表示结束,
     * 用 char 接收会把 EOF 和某个字符混淆。 */
    while ((c = fgetc(fp)) != EOF) {
        chars++;
        if (c == '\n') {
            lines++;
        }
        if (c == ' ' || c == '\t' || c == '\n') {
            in_word = 0;                /* 遇到空白, 单词结束 */
        } else if (!in_word) {
            in_word = 1;
            words++;                    /* 从空白进入非空白, 说明新单词开始 */
        }
    }

    fclose(fp);
    printf("  字符=%ld 行=%ld 词=%ld\n", chars, lines, words);
    puts("  注意: 这里按字节统计, 一个 UTF-8 汉字算 3 个字节。");
}

/* ==================== 5. 二进制读写 ==================== */
static void write_read_binary(void)
{
    puts("\n========== 5. 二进制读写 ==========");

    Record recs[3] = {
        {1, "Tom",   91.5},
        {2, "Jerry", 78.0},
        {3, "Spike", 85.5},
    };

    /* --- 写 --- */
    FILE *fp = fopen(BIN_FILE, "wb");   /* "b" 在 Linux 上无差别, 但写明更清晰 */
    if (fp == NULL) {
        perror("fopen");
        return;
    }
    size_t written = fwrite(recs, sizeof(Record), 3, fp);
    fclose(fp);
    printf("  fwrite 写入 %zu 条记录, 每条 %zu 字节\n", written, sizeof(Record));

    /* --- 读 --- */
    fp = fopen(BIN_FILE, "rb");
    if (fp == NULL) {
        perror("fopen");
        return;
    }
    Record back[3];
    size_t got = fread(back, sizeof(Record), 3, fp);    /* 返回值是"读到了几条" */
    fclose(fp);

    printf("  fread 读回 %zu 条记录:\n", got);
    for (size_t i = 0; i < got; i++) {
        printf("    id=%d name=%-8s score=%.1f\n",
               back[i].id, back[i].name, back[i].score);
    }
    puts("  二进制读写直接照搬内存, 快; 但结构体布局/字节序变了就读不出来了。");
}

/* ==================== 6. 定位: fseek / ftell ==================== */
static void seek_demo(void)
{
    puts("\n========== 6. fseek / ftell / rewind ==========");

    FILE *fp = fopen(BIN_FILE, "rb");
    if (fp == NULL) {
        perror("fopen");
        return;
    }

    /* 求文件大小: 跳到末尾, 看偏移量 */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    printf("  文件大小 = %ld 字节 = %ld 条记录\n", size, size / (long)sizeof(Record));

    /* 直接跳到第 2 条记录, 只读它 */
    fseek(fp, (long)sizeof(Record), SEEK_SET);      /* 偏移 1 条记录的位置 */
    Record r;
    if (fread(&r, sizeof(Record), 1, fp) == 1) {
        printf("  直接跳到第 2 条: id=%d name=%s score=%.1f\n",
               r.id, r.name, r.score);
    }

    /* 回到开头 */
    rewind(fp);
    printf("  rewind 之后 ftell = %ld\n", ftell(fp));

    fclose(fp);

    puts("\n  SEEK_SET 从开头算, SEEK_CUR 从当前位置算, SEEK_END 从末尾算。");
}

/* ==================== 7. 杂项 ==================== */
static void misc(void)
{
    puts("\n========== 7. 其它常用操作 ==========");

    /* 判断文件是否存在 */
    FILE *fp = fopen(TXT_FILE, "r");
    if (fp != NULL) {
        puts("  demo_11_text.txt 存在");
        fclose(fp);
    }

    /* 删除文件 */
    if (remove(TXT_FILE) == 0) {
        printf("  已删除 %s\n", TXT_FILE);
    } else {
        perror("remove");
    }
    if (remove(BIN_FILE) == 0) {
        printf("  已删除 %s\n", BIN_FILE);
    } else {
        perror("remove");
    }

    /* 重命名: rename(old, new) */
    puts("\n  其它常用函数:");
    puts("    rename(old, new)   改名/移动");
    puts("    remove(path)       删除");
    puts("    tmpfile()          临时文件, 关闭时自动删除");
    puts("    freopen()          重定向流(例如把 stdout 指到文件)");
    puts("    fflush(fp)         立刻把缓冲区刷到磁盘");
    puts("    ferror(fp)         检查之前的操作有没有出错");
}

int main(void)
{
    write_text();
    read_text();
    append_text();
    count_file();
    write_read_binary();
    seek_demo();
    misc();

    puts("\n注意: 读文件前一定要判断 fopen 是否返回 NULL ——");
    puts("      文件不存在、没权限、路径写错, 它都会返回 NULL 而不是报错。");
    return 0;
}
