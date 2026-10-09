/*
 * steps_demo.c —— 只跑"配置一个模式"的这几步, 每一步都把寄存器打出来
 *
 *   步骤1 查表:     "uart" 是几号功能? "uart-0" 是哪几个脚?     (不碰寄存器)
 *   步骤2 写 MUX:   整组逐脚, 每个脚只动 4 个 bit
 *   步骤3 写电气:   PULL / DRIVE / OE, 同一个脚的另一组位
 *   步骤4 看结果:   最终寄存器 + "整字覆盖"的反面教材
 *
 * 目标: 让 uart-0 这组脚(PA0/PA1)工作在 uart, PA0 再配上拉 + 8mA + 输出使能
 *
 * 编译运行:  make steps
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

/* ==== 真正写寄存器的就这一个函数: 清掉字段 -> 填新值 ==== */
static void pin_update(unsigned int pin, unsigned int mask, unsigned int shift,
		       unsigned int val)
{
	pin_reg[pin] &= ~(mask << shift);	/* 清字段 */
	pin_reg[pin] |= (val & mask) << shift;	/* 填新值 */
}

/* ===================== 步骤1 要用的表: 组 ===================== */
struct group {
	const char		*name;
	const unsigned int	*pins;
	unsigned int		 npins;
};

static const unsigned int uart_pins[] = { 0, 1 };	/* PA0=TX PA1=RX */

static const struct group groups[] = {
	{ "uart-0", uart_pins, 2 },
};
#define NGROUPS	1

/* ---- 查表: 名字 -> 编号 (真实内核里就是这些表, 只是从 dts 读出来) ---- */
static int func_lookup(const char *name)
{
	unsigned int i;

	for (i = 0; i < sizeof(func_names) / sizeof(func_names[0]); i++)
		if (strcmp(func_names[i], name) == 0)
			return (int)i;
	return -1;
}

static const struct group *group_lookup(const char *name)
{
	unsigned int i;

	for (i = 0; i < NGROUPS; i++)
		if (strcmp(groups[i].name, name) == 0)
			return &groups[i];
	return NULL;
}

/* ============ 一次字段写入 + 把"动了哪几位"打出来 ============ */
static unsigned int top_bit(unsigned int mask)
{
	unsigned int b = 0;

	while (mask >>= 1)
		b++;
	return b;
}

static void apply(const char *what, unsigned int pin, unsigned int mask,
		  unsigned int shift, unsigned int val)
{
	unsigned int before = pin_reg[pin];
	int i;

	pin_update(pin, mask, shift, val);

	printf("  %s %-11s 0x%08x -> 0x%08x   字段 bit%u:%u, 实际变了 ",
	       pin_names[pin], what, before, pin_reg[pin], shift + top_bit(mask),
	       shift);
	for (i = 31; i >= 0; i--)
		if (((before >> i) & 1) != ((pin_reg[pin] >> i) & 1))
			printf("bit%d ", i);
	printf("\n");
}

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

int main(void)
{
	const struct group *g;
	int fsel;
	unsigned int i;

	puts("==============================================================");
	puts(" 配置一个模式: uart-0 组(PA0/PA1) 工作在 uart,");
	puts(" PA0 另配上拉 + 8mA + 输出使能");
	puts("==============================================================");

	/* ---------------- 步骤1 查表 ---------------- */
	puts("\n【步骤1】查表: 名字 -> 编号/引脚  (纯查表, 一个寄存器都不碰)");
	fsel = func_lookup("uart");
	g = group_lookup("uart-0");
	if (fsel < 0 || !g) {
		puts("  查不到 -> 直接报错返回 (dts 里写错名字就是这个下场)");
		return 1;
	}
	printf("  \"uart\"   -> 功能编号 %d\n", fsel);
	printf("  \"uart-0\" -> 一组脚:");
	for (i = 0; i < g->npins; i++)
		printf(" %s", pin_names[g->pins[i]]);
	printf("   (组内脚号:");
	for (i = 0; i < g->npins; i++)
		printf(" %u", g->pins[i]);
	puts(")");
	printf("  此刻寄存器: PA0=0x%08x PA1=0x%08x —— 还是复位值\n",
	       pin_reg[0], pin_reg[1]);

	/* ---------------- 步骤2 写 MUX ---------------- */
	puts("\n【步骤2】写 MUX: 整组逐脚, 每个脚只动 4 个 bit (bit3:0)");
	for (i = 0; i < g->npins; i++)
		apply("MUX=uart", g->pins[i], MUX_MASK, MUX_SHIFT,
		      (unsigned int)fsel);
	puts("  一组脚必须一次切完, 否则会出现半 uart 半 gpio 的中间态");

	/* ---------------- 步骤3 写电气参数 ---------------- */
	puts("\n【步骤3】写电气参数: 同一个脚, 换另一组位 (真实硬件是另一个寄存器)");
	apply("PULL=上拉", 0, PULL_MASK, PULL_SHIFT, PULL_UP);
	apply("DRIVE=8mA", 0, DRIVE_MASK, DRIVE_SHIFT, 8);
	apply("OE=1", 0, 1, OE_SHIFT, 1);
	puts("  注意: 上拉只动了 bit5:4, 驱动只动了 bit11:8, 互不影响");

	/* ---------------- 步骤4 看结果 ---------------- */
	puts("\n【步骤4】结果:");
	dump_all();

	/* ---------------- 反面: 整字覆盖 ---------------- */
	puts("\n【反面】如果偷懒写成 pin_reg[0] = 1 (整字覆盖):");
	printf("  覆盖前 PA0 = 0x%08x\n", pin_reg[0]);
	pin_reg[0] = 1;
	printf("  覆盖后 PA0 = 0x%08x   -> 上拉/驱动/OE 全丢了\n", pin_reg[0]);
	puts("  pin_update() 存在的理由就是这个: 只动自己的字段");

	return 0;
}
