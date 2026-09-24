/*
 * 10_linked_list.c —— 单链表
 *
 * 数组: 内存连续, 随机访问快(O(1)), 但中间插入删除要搬数据(O(n))。
 * 链表: 内存不连续, 随机访问慢(O(n)), 但插入删除只改指针(O(1))。
 *
 * 链表是"指针 + 结构体 + 动态内存"的综合练习, 也是面试高频题。
 *
 * 节点定义:
 *     +--------+------+      +--------+------+      +--------+------+
 *     | value  | next | ---> | value  | next | ---> | value  | next | ---> NULL
 *     +--------+------+      +--------+------+      +--------+------+
 */
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int          value;
    struct Node *next;      /* 注意: 这里用 struct Node*(此时 typedef 名还没生效) */
} Node;

/* ==================== 创建与插入 ==================== */

static Node *node_new(int value)
{
    Node *n = malloc(sizeof(Node));
    if (n == NULL) {
        return NULL;
    }
    n->value = value;
    n->next  = NULL;
    return n;
}

/* 头插: O(1)。返回新的头指针 */
static Node *push_front(Node *head, int value)
{
    Node *n = node_new(value);
    if (n == NULL) {
        return head;
    }
    n->next = head;
    return n;
}

/* 尾插: O(n), 因为要先走到最后一个节点 */
static Node *push_back(Node *head, int value)
{
    Node *n = node_new(value);
    if (n == NULL) {
        return head;
    }
    if (head == NULL) {
        return n;                   /* 空链表, 新节点就是头 */
    }

    Node *cur = head;
    while (cur->next != NULL) {
        cur = cur->next;
    }
    cur->next = n;
    return head;
}

/* 在第一个值为 target 的节点后面插入 */
static void insert_after(Node *head, int target, int value)
{
    for (Node *cur = head; cur != NULL; cur = cur->next) {
        if (cur->value == target) {
            Node *n = node_new(value);
            if (n == NULL) {
                return;
            }
            n->next = cur->next;    /* 先接后面, 再断前面 —— 顺序不能反 */
            cur->next = n;
            return;
        }
    }
    printf("  没找到 %d, 插入失败\n", target);
}

/* ==================== 查找与统计 ==================== */

static Node *list_find(Node *head, int value)
{
    for (Node *cur = head; cur != NULL; cur = cur->next) {
        if (cur->value == value) {
            return cur;             /* 返回节点地址, 调用者可以继续操作它 */
        }
    }
    return NULL;
}

static int list_length(const Node *head)
{
    int n = 0;
    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        n++;
    }
    return n;
}

/* 递归统计长度: 和上面功能一样, 体会两种写法 */
static int list_length_rec(const Node *head)
{
    if (head == NULL) {
        return 0;                   /* 出口 */
    }
    return 1 + list_length_rec(head->next);
}

/* ==================== 删除 ==================== */

/* 删除所有值为 value 的节点。
 * 因为可能要删掉头结点, 头指针会变, 所以必须返回新的头指针。 */
static Node *list_remove(Node *head, int value)
{
    /* 先处理"头结点本身就要删"的情况, 而且要连着处理多个 */
    while (head != NULL && head->value == value) {
        Node *dead = head;
        head = head->next;
        free(dead);
    }
    if (head == NULL) {
        return NULL;
    }

    /* 再用 cur->next 判断, 这样能拿到"待删节点的前驱", 方便摘链 */
    Node *cur = head;
    while (cur->next != NULL) {
        if (cur->next->value == value) {
            Node *dead = cur->next;
            cur->next = dead->next;     /* 跳过 dead, 把它从链上摘掉 */
            free(dead);
        } else {
            cur = cur->next;            /* 注意: 删除时不要往前挪, 免得漏掉连续重复 */
        }
    }
    return head;
}

/* ==================== 反转 ==================== */

/* 三指针原地反转: 把每个节点的 next 指向它的前驱 */
static Node *list_reverse(Node *head)
{
    Node *prev = NULL;
    Node *cur  = head;

    while (cur != NULL) {
        Node *next = cur->next;     /* 1. 先记住后面, 否则改完 next 就找不到了 */
        cur->next  = prev;          /* 2. 掉头 */
        prev = cur;                 /* 3. 前驱前进 */
        cur  = next;                /* 4. 当前前进 */
    }
    return prev;                    /* 循环结束时 prev 就是新的头 */
}

/* ==================== 输出与释放 ==================== */

static void list_print(const char *tag, const Node *head)
{
    printf("  %s: ", tag);
    for (const Node *cur = head; cur != NULL; cur = cur->next) {
        printf("%d", cur->value);
        if (cur->next != NULL) {
            printf(" -> ");
        }
    }
    printf(" -> NULL   (共 %d 个节点)\n", list_length(head));
}

static void list_free(Node *head)
{
    while (head != NULL) {
        Node *next = head->next;    /* 先存下一个, 再 free 当前 */
        free(head);
        head = next;
    }
}

int main(void)
{
    /* ==================== 1. 尾插构建 ==================== */
    puts("========== 1. 尾插构建链表 ==========");

    Node *head = NULL;
    for (int i = 1; i <= 5; i++) {
        head = push_back(head, i * 10);
    }
    list_print("尾插 10..50", head);

    /* ==================== 2. 头插 ==================== */
    puts("\n========== 2. 头插(顺序会反过来) ==========");
    Node *h2 = NULL;
    for (int i = 1; i <= 5; i++) {
        h2 = push_front(h2, i * 10);
    }
    list_print("头插 10..50", h2);
    list_free(h2);

    /* ==================== 3. 插入 ==================== */
    puts("\n========== 3. 在指定值后面插入 ==========");
    insert_after(head, 30, 35);
    list_print("在 30 后插入 35", head);
    insert_after(head, 999, 1);         /* 故意插一个不存在的值 */

    /* ==================== 4. 查找与长度 ==================== */
    puts("\n========== 4. 查找与长度 ==========");
    Node *found = list_find(head, 35);
    if (found != NULL) {
        printf("  找到 %d, 它的后继节点是 %s\n",
               found->value, found->next ? "存在的节点" : "NULL");
        found->value = 350;             /* 通过找到的指针直接改数据 */
        list_print("改 35 -> 350", head);
    } else {
        puts("  没找到 35");
    }
    if (list_find(head, 999) == NULL) {
        puts("  999 不在链表里");
    }
    printf("  迭代求长度 = %d, 递归求长度 = %d\n",
           list_length(head), list_length_rec(head));

    /* ==================== 5. 反转 ==================== */
    puts("\n========== 5. 原地反转(链表经典题) ==========");
    list_print("反转前", head);
    head = list_reverse(head);
    list_print("反转后", head);
    head = list_reverse(head);          /* 再转回来 */
    list_print("再反转一次", head);

    /* ==================== 6. 删除 ==================== */
    puts("\n========== 6. 删除节点 ==========");
    head = push_front(head, 10);        /* 让 10 出现两次 */
    list_print("准备删除 10", head);

    head = list_remove(head, 10);
    list_print("删除所有 10", head);

    head = list_remove(head, 350);
    list_print("删除 350", head);

    head = list_remove(head, 12345);
    list_print("删除不存在的 12345", head);

    /* ==================== 7. 释放 ==================== */
    puts("\n========== 7. 释放整条链表 ==========");
    list_free(head);                    /* 必须逐个 free, 否则整条链全部泄漏 */
    head = NULL;
    puts("  已全部释放, 并把 head 置为 NULL");

    puts("\n  链表小结:");
    puts("    - 头插 O(1), 尾插 O(n)(除非维护尾指针)");
    puts("    - 删除/插入要修改前驱的 next, 所以常常需要用 cur->next 来遍历");
    puts("    - 删除头结点会改变头指针, 所以函数要返回新头(或用二级指针)");
    return 0;
}
