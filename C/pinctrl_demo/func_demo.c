/*
 * func_demo.c —— 每个函数单独调用一遍, 看它到底做什么
 *
 *   一个函数一个 demo_xxx(): 只干一件小事, 输出里写清 "输入 -> 输出"。
 *   整体流程在 steps_demo.c, 这里是把里面的函数拆开逐个试。
 *
 * 编译运行:  make func
 */
#include <stdio.h>
#include <string.h>

/* ===================== 硬件: 一个脚一个 32 位寄存器 ===================== */
#define NPINS		4

#define MUX_SHIFT	0
#define MUX_MASK	0xf
#define PULL_SHIFT	4
#define PULL_MASK	0x3
#define DRIVE_SHIFT	8
#define DRIVE_MASK	0xf
#define OE_SHIFT	16

#define PULL_FLOAT	0
#define PULL_UP		1
#define PULL_DOWN	2

static unsigned int pin_reg[NPINS];

static const char * const pin_names[NPINS] = { "PA0", "PA1", "PA2", "PA3" };
static const char * const func_names[]      = { "gpio", "uart", "i2c" };

/* ===================== 组表 ===================== */
struct group {
	const char		*name;
	const unsigned int	*pins;
	unsigned int		 npins;
};

static const unsigned int uart_pins[] = { 0, 1 };
static const unsigned int i2c_pins[]  = { 2, 3 };

static const struct group groups[] = {
	{ "uart-0", uart_pins, 2 },
	{ "i2c-1",  i2c_pins,  2 },
};
#define NGROUPS	2

/* ==================================================================== */
/*  函数1  pin_update —— 改寄存器里的一个字段                            */
/*         &~ 清掉字段, |= 填新值; 别的位一动不动                        */
/* ==================================================================== */
static void pin_update(unsigned int pin, unsigned int mask, unsigned int shift,
		       unsigned int val)
{
	pin_reg[pin] &= ~(mask << shift);
	pin_reg[pin] |= (val & mask) << shift;
}

/* 每个 demo 都从"干净寄存器"开始 */
static void reset_regs(void)
{
	memset(pin_reg, 0, sizeof(pin_reg));
}

/* 打印一次变化 */
static void show(unsigned int before)
{
	printf("           0x%08x -> 0x%08x\n", before, pin_reg[0]);
}

static void demo_pin_update(void)
{
	unsigned int b;

	puts("──────────────────────────────────────────────────────────────");
	puts("【函数1】pin_update(pin, mask, shift, val)   改一个字段");
	puts("──────────────────────────────────────────────────────────────");
	reset_regs();

	puts("  ① pin_update(0, 0xf, 0, 1)      MUX = 1 (uart)");
	b = pin_reg[0];
	pin_update(0, MUX_MASK, MUX_SHIFT, 1);
	show(b);

	puts("  ② pin_update(0, 0x3, 4, 1)      PULL = 1 (上拉)");
	b = pin_reg[0];
	pin_update(0, PULL_MASK, PULL_SHIFT, PULL_UP);
	show(b);

	puts("  ③ pin_update(0, 0xf, 8, 8)      DRIVE = 8 (mA)");
	b = pin_reg[0];
	pin_update(0, DRIVE_MASK, DRIVE_SHIFT, 8);
	show(b);

	puts("  ④ pin_update(0, 1, 16, 1)       OE = 1");
	b = pin_reg[0];
	pin_update(0, 1, OE_SHIFT, 1);
	show(b);

	puts("  ⑤ pin_update(0, 0xf, 8, 0x1f)   多传的位被 & mask 截掉, 只写 0xf");
	b = pin_reg[0];
	pin_update(0, DRIVE_MASK, DRIVE_SHIFT, 0x1f);
	show(b);

	puts("  ⑥ pin_update(0, 0xf, 0, 0)      MUX 改回 gpio (其他字段还在)");
	b = pin_reg[0];
	pin_update(0, MUX_MASK, MUX_SHIFT, 0);
	show(b);
	puts("  → 一次只碰一个字段, 所以 ⑤⑥ 怎么折腾都不会影响别的字段");
	puts("");
}

/* ==================================================================== */
/*  函数2  top_bit —— 算字段最高位, 只为了打印 "bit3:0" 这种好看的范围   */
/* ==================================================================== */
static unsigned int top_bit(unsigned int mask)
{
	unsigned int b = 0;

	while (mask >>= 1)
		b++;
	return b;
}

static void demo_top_bit(void)
{
	puts("──────────────────────────────────────────────────────────────");
	puts("【函数2】top_bit(mask)   算字段宽度, 纯打印用, 不碰寄存器");
	puts("──────────────────────────────────────────────────────────────");
	printf("  top_bit(0xf) = %u  -> 字段 bit3:0   (MUX)\n", top_bit(MUX_MASK));
	printf("  top_bit(0x3) = %u  -> 字段 bit5:4   (PULL)\n", top_bit(PULL_MASK));
	printf("  top_bit(0x1) = %u  -> 字段 bit16:16 (OE)\n", top_bit(1));
	puts("");
}

/* ==================================================================== */
/*  函数3  func_lookup —— 功能名 -> 编号, 查不到返回 -1                  */
/* ==================================================================== */
static int func_lookup(const char *name)
{
	unsigned int i;

	for (i = 0; i < sizeof(func_names) / sizeof(func_names[0]); i++)
		if (strcmp(func_names[i], name) == 0)
			return (int)i;
	return -1;
}

static void demo_func_lookup(void)
{
	static const char * const test[] = { "gpio", "uart", "i2c", "spi" };
	unsigned int i;

	puts("──────────────────────────────────────────────────────────────");
	puts("【函数3】func_lookup(name)   功能名 -> 功能编号");
	puts("──────────────────────────────────────────────────────────────");
	for (i = 0; i < sizeof(test) / sizeof(test[0]); i++) {
		int sel = func_lookup(test[i]);

		if (sel < 0)
			printf("  func_lookup(\"%s\") = %d   ← 没这个功能, 上层直接报错\n",
			       test[i], sel);
		else
			printf("  func_lookup(\"%s\") = %d\n", test[i], sel);
	}
	puts("");
}

/* ==================================================================== */
/*  函数4  group_lookup —— 组名 -> 组(含成员引脚), 查不到返回 NULL        */
/* ==================================================================== */
static const struct group *group_lookup(const char *name)
{
	unsigned int i;

	for (i = 0; i < NGROUPS; i++)
		if (strcmp(groups[i].name, name) == 0)
			return &groups[i];
	return NULL;
}

static void demo_group_lookup(void)
{
	static const char * const test[] = { "uart-0", "i2c-1", "spi-0" };
	unsigned int i, j;

	puts("──────────────────────────────────────────────────────────────");
	puts("【函数4】group_lookup(name)   组名 -> 组, 并列出成员脚");
	puts("──────────────────────────────────────────────────────────────");
	for (i = 0; i < sizeof(test) / sizeof(test[0]); i++) {
		const struct group *g = group_lookup(test[i]);

		if (!g) {
			printf("  group_lookup(\"%s\") = NULL   ← 组不存在\n", test[i]);
			continue;
		}
		printf("  group_lookup(\"%s\") = 一组脚:", test[i]);
		for (j = 0; j < g->npins; j++)
			printf(" %s", pin_names[g->pins[j]]);
		printf("     (组内脚号:");
		for (j = 0; j < g->npins; j++)
			printf(" %u", g->pins[j]);
		puts(")");
	}
	puts("  → 查表只翻译名字, 一个寄存器都不碰");
	puts("");
}

/* ==================================================================== */
/*  函数5  apply —— 写一次字段, 并打印"字段范围 + 哪几位真的变了"          */
/*                  它内部调用 pin_update                                */
/* ==================================================================== */
static void apply(const char *what, unsigned int pin, unsigned int mask,
		  unsigned int shift, unsigned int val)
{
	unsigned int before = pin_reg[pin];
	int i, changed = 0;

	pin_update(pin, mask, shift, val);

	printf("  %s %-11s 0x%08x -> 0x%08x   字段 bit%u:%u, 实际变了 ",
	       pin_names[pin], what, before, pin_reg[pin], shift + top_bit(mask),
	       shift);
	for (i = 31; i >= 0; i--)
		if (((before >> i) & 1) != ((pin_reg[pin] >> i) & 1)) {
			printf("bit%d ", i);
			changed = 1;
		}
	if (!changed)
		printf("无(值跟原来一样)");
	printf("\n");
}

static void demo_apply(void)
{
	puts("──────────────────────────────────────────────────────────────");
	puts("【函数5】apply(名字, pin, mask, shift, val)   写 + 打印变化");
	puts("──────────────────────────────────────────────────────────────");
	reset_regs();
	puts("  先把寄存器清零, 然后两次调用:");
	puts("  apply(\"MUX=uart\", 0, 0xf, 0, 1)  ->");
	apply("MUX=uart", 0, MUX_MASK, MUX_SHIFT, 1);
	puts("  apply(\"PULL=上拉\", 0, 0x3, 4, 1)  ->");
	apply("PULL=上拉", 0, PULL_MASK, PULL_SHIFT, PULL_UP);
	puts("  apply(\"PULL=上拉\", 0, 0x3, 4, 1)  再来一次 ->");
	apply("PULL=上拉", 0, PULL_MASK, PULL_SHIFT, PULL_UP);
	puts("");
}

/* ==================================================================== */
/*  函数6  dump_all —— 把每个脚的字段解码打出来                          */
/* ==================================================================== */
static void dump_all(void)
{
	static const char * const pull_names[] = { "悬空", "上拉", "下拉" };
	unsigned int i;

	for (i = 0; i < NPINS; i++) {
		unsigned int r = pin_reg[i];
		unsigned int mux = (r >> MUX_SHIFT) & MUX_MASK;

		printf("  %s: MUX=%-4s PULL=%-4s DRIVE=%2umA OE=%u   (0x%08x)\n",
		       pin_names[i],
		       mux < 3 ? func_names[mux] : "?",
		       pull_names[(r >> PULL_SHIFT) & PULL_MASK],
		       (r >> DRIVE_SHIFT) & DRIVE_MASK,
		       (r >> OE_SHIFT) & 1, r);
	}
}

static void demo_dump_all(void)
{
	puts("──────────────────────────────────────────────────────────────");
	puts("【函数6】dump_all()   解码打印, 不写任何东西");
	puts("──────────────────────────────────────────────────────────────");
	reset_regs();
	pin_update(0, MUX_MASK, MUX_SHIFT, 1);
	pin_update(0, PULL_MASK, PULL_SHIFT, PULL_UP);
	pin_update(1, MUX_MASK, MUX_SHIFT, 1);
	puts("  先手工写 3 个字段, 再看整颗状态:");
	dump_all();
	puts("");
}

/* ==================================================================== */
/*  函数7  反面: 直接画等号 —— 整字覆盖会连累别的字段                     */
/* ==================================================================== */
static void demo_direct_write(void)
{
	unsigned int b;

	puts("──────────────────────────────────────────────────────────────");
	puts("【反面】pin_reg[0] = 1    直接赋值 = 把整个寄存器擦掉重写");
	puts("──────────────────────────────────────────────────────────────");
	reset_regs();
	pin_update(0, MUX_MASK, MUX_SHIFT, 1);
	pin_update(0, PULL_MASK, PULL_SHIFT, PULL_UP);
	pin_update(0, DRIVE_MASK, DRIVE_SHIFT, 8);
	pin_update(0, 1, OE_SHIFT, 1);
	printf("  辛苦配好: PA0 = 0x%08x\n", pin_reg[0]);

	b = pin_reg[0];
	pin_reg[0] = 1;			/* 只想要 MUX, 结果全丢 */
	printf("  pin_reg[0] = 1 之后: %s  0x%08x -> 0x%08x\n",
	       pin_names[0], b, pin_reg[0]);
	puts("  → 上拉/驱动/OE 全没了, 而且没有任何报错 —— 这就是 pin_update 存在的理由");
	puts("");
}

/* ==================================================================== */
int main(void)
{
	puts("==============================================================");
	puts(" 每个函数单独调用一遍: 输入是什么, 输出是什么");
	puts("==============================================================");
	puts("");

	demo_pin_update();	/* 函数1: 唯一真的写寄存器的 */
	demo_top_bit();		/* 函数2 */
	demo_func_lookup();	/* 函数3 */
	demo_group_lookup();	/* 函数4 */
	demo_apply();		/* 函数5: 写 + 打印 */
	demo_dump_all();	/* 函数6 */
	demo_direct_write();	/* 反面 */

	reset_regs();
	pin_update(0, MUX_MASK, MUX_SHIFT, 1);
	puts("──────────────────────────────────────────────────────────────");
	puts(" 一句话: 查表(func_lookup/group_lookup) 拿编号, pin_update 写字段,");
	puts("        apply/dump_all 只负责让你看见 —— 写寄存器只有 pin_update 一处。");
	puts("──────────────────────────────────────────────────────────────");
	return 0;
}
