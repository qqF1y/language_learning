/*
 * pinctrl_demo.c —— 一个文件讲清 pinctrl 子系统在干什么
 *
 * ══ 它要解决的问题 ══
 * 一颗 SoC 上百个引脚, 每个脚背后就那么一个寄存器, 里面的几段位决定:
 *     这个脚接给谁用(mux)? 要不要内部上拉? 驱动能力几毫安? 输出使能了吗?
 *
 * 如果每个驱动自己去写这几段位, 一定会出事:
 *     串口驱动把 PA0 设成 UART ──► 十分钟后 I2C 驱动又把 PA0 设成 I2C
 *     ──► 串口突然不工作了, 而且没人知道是谁改的。
 *
 * 所以内核定了规矩: **别人不许直接碰这些寄存器**。要改引脚, 只能通过
 * pin controller 的三层抽象提出申请:
 *
 *        pin(引脚)    ──►   group(一起切换的一组脚)   ──►   function(这组脚干什么)
 *          PA0                 uart-0                          uart
 *          PA1                 uart-0                          uart
 *
 * 这个文件用一个"软件版的 pinctrl 核心"把机制跑一遍, 每一步都打印出来:
 *   ① 注册 pin controller: 报户口(有几个脚/几组/几种功能)
 *   ② 消费者申请 state:    核心查表 → 调驱动的 set_mux()
 *   ③ pinconf:             改电气参数(上拉/驱动能力/OE)
 *   ④ strict 排他:         两组人抢同一组脚 → EBUSY
 *   ⑤ 对比:                没有 pinctrl 的世界会怎么坏
 *
 * 编译运行:  make run      (或 gcc -Wall -Wextra pinctrl_demo.c -o pinctrl_demo)
 */
#include <stdio.h>
#include <string.h>

/* =====================================================================
 *  1. 硬件: 每个引脚一个 32 位寄存器
 *
 *      bit  0-3  MUX    复用选择: 0=gpio 1=uart 2=i2c
 *      bit  4-5  PULL   上下拉:   0=悬空 1=上拉 2=下拉
 *      bit  8-11 DRIVE  驱动能力(mA)
 *      bit 16    OE     输出使能
 * ===================================================================*/
#define NPINS		4

#define MUX_SHIFT	0
#define MUX_MASK	0xf
#define PULL_SHIFT	4
#define PULL_MASK	0x3
#define DRIVE_SHIFT	8
#define DRIVE_MASK	0xf
#define OE_SHIFT	16

#define FUNC_GPIO	0
#define FUNC_UART	1
#define FUNC_I2C	2
#define FUNC_NR		3

#define PULL_FLOAT	0
#define PULL_UP		1
#define PULL_DOWN	2

/* 内核里错误码习惯用"负的 errno": -EBUSY 表示"东西正被别人占着" */
#define EBUSY		16

static unsigned int pin_reg[NPINS];	/* 假装这就是那块 IOMUX 寄存器区 */

static void pin_update(unsigned int pin, unsigned int mask, unsigned int shift,
		       unsigned int val)
{
	pin_reg[pin] &= ~(mask << shift);
	pin_reg[pin] |= (val & mask) << shift;
}

static const char * const func_names[FUNC_NR] = { "gpio", "uart", "i2c" };

static void pin_dump(const char *tag)
{
	unsigned int i;

	printf("      [寄存器] %s\n", tag);
	for (i = 0; i < NPINS; i++) {
		unsigned int r = pin_reg[i];
		unsigned int mux = (r >> MUX_SHIFT) & MUX_MASK;
		unsigned int pull = (r >> PULL_SHIFT) & PULL_MASK;
		static const char * const pull_names[] = { "悬空", "上拉", "下拉" };

		printf("        PA%u: MUX=%-4s PULL=%-4s DRIVE=%2umA OE=%u   (0x%08x)\n",
		       i, mux < FUNC_NR ? func_names[mux] : "?",
		       pull_names[pull], (r >> DRIVE_SHIFT) & DRIVE_MASK,
		       (r >> OE_SHIFT) & 1, r);
	}
}

/* =====================================================================
 *  2. 驱动侧: 户口本 + 两个回调
 *     —— "pin controller 驱动"要提供的东西就这些
 * ===================================================================*/
struct pin_desc {
	unsigned int	number;
	const char	*name;
};

struct pin_group {			/* 一组脚: 功能切换的最小单位 */
	const char		*name;
	const unsigned int	*pins;
	unsigned int		 npins;
};

static const struct pin_desc pins[NPINS] = {
	{ 0, "PA0" }, { 1, "PA1" }, { 2, "PA2" }, { 3, "PA3" },
};

static const unsigned int uart_pins[] = { 0, 1 };	/* PA0=TX PA1=RX */
static const unsigned int i2c_pins[]  = { 2, 3 };	/* PA2=SCL PA3=SDA */

static const struct pin_group groups[] = {
	{ "uart-0", uart_pins, 2 },
	{ "i2c-1",  i2c_pins,  2 },
};
#define NGROUPS	2

/* 每种功能能挂在哪些组上(gpio 可以挂所有组: 不用的脚都能当 gpio)\n * 用 NULL 结尾表示结束, 比多维护一个"有几组"的计数少一个出错的地方。 */
static const char * const gpio_groups[] = { "uart-0", "i2c-1", NULL };
static const char * const uart_groups[] = { "uart-0", NULL };
static const char * const i2c_groups[]  = { "i2c-1", NULL };

/* ---- 回调①: 切功能 ---- */
static int demo_set_mux(unsigned int func, unsigned int grp)
{
	const struct pin_group *g = &groups[grp];
	unsigned int i;

	printf("      [驱动] set_mux(function=%s, group=%s): 整组一起切\n",
	       func_names[func], g->name);

	/* 一组脚必须一起切: 分两步会出现一瞬间"半 UART 半 I2C"的状态,
	 * 对面外设会收到垃圾。 */
	for (i = 0; i < g->npins; i++) {
		pin_update(g->pins[i], MUX_MASK, MUX_SHIFT, func);
		printf("             %s.MUX = %s\n", pins[g->pins[i]].name,
		       func_names[func]);
	}
	return 0;
}

/* ---- 回调②: 改电气参数 ---- */
#define PARAM_BIAS_DISABLE	1
#define PARAM_BIAS_PULL_DOWN	3
#define PARAM_BIAS_PULL_UP	5
#define PARAM_DRIVE_STRENGTH	9
#define PARAM_OUTPUT_ENABLE	18

static int demo_pin_config_set(unsigned int pin, unsigned int param,
			       unsigned int arg)
{
	if (pin >= NPINS)
		return -1;

	switch (param) {
	case PARAM_BIAS_DISABLE:
		pin_update(pin, PULL_MASK, PULL_SHIFT, PULL_FLOAT);
		printf("      [驱动] %s 上下拉 = 悬空\n", pins[pin].name);
		break;
	case PARAM_BIAS_PULL_UP:
		pin_update(pin, PULL_MASK, PULL_SHIFT, PULL_UP);
		printf("      [驱动] %s 上下拉 = 上拉\n", pins[pin].name);
		break;
	case PARAM_BIAS_PULL_DOWN:
		pin_update(pin, PULL_MASK, PULL_SHIFT, PULL_DOWN);
		printf("      [驱动] %s 上下拉 = 下拉\n", pins[pin].name);
		break;
	case PARAM_DRIVE_STRENGTH:
		pin_update(pin, DRIVE_MASK, DRIVE_SHIFT, arg);
		printf("      [驱动] %s 驱动能力 = %u mA\n", pins[pin].name, arg);
		break;
	case PARAM_OUTPUT_ENABLE:
		pin_update(pin, 1, OE_SHIFT, arg ? 1 : 0);
		printf("      [驱动] %s 输出使能 = %u\n", pins[pin].name, arg ? 1 : 0);
		break;
	default:
		/* 认不出的参数要明确说不支持, 别默默忽略 —— 用户会以为设成功了 */
		printf("      [驱动] 不支持的参数 %u, 返回错误\n", param);
		return -1;
	}
	return 0;
}

/* =====================================================================
 *  3. 核心侧: 内核里的 pinctrl 子系统
 *     它只认下面这一张"描述表", 外加驱动的两个回调。
 *     真实内核的 struct pinctrl_desc / pinmux_ops / pinconf_ops 就是这个形状。
 * ===================================================================*/
struct pinmux_ops {
	int (*set_mux)(unsigned int func_selector, unsigned int group_selector);
};

struct pinconf_ops {
	int (*pin_config_set)(unsigned int pin, unsigned int param,
			      unsigned int arg);
};

struct pinctrl_desc {
	const char			*name;
	const struct pin_desc		*pins;
	unsigned int			 npins;
	const struct pin_group		*groups;	/* 户口本: 有几组、每组是谁 */
	unsigned int			 ngroups;
	const char * const * const	*function_groups;	/* 每种功能挂哪些组 */
	unsigned int			 nfunctions;
	const struct pinmux_ops		*pmxops;
	const struct pinconf_ops	*confops;
	unsigned int			 strict;	/* 1 = 排他, 一组脚只能一个功能 */
};

static const struct pinmux_ops demo_pmxops = { .set_mux = demo_set_mux };
static const struct pinconf_ops demo_confops = {
	.pin_config_set = demo_pin_config_set
};

static const char * const * const demo_function_groups[FUNC_NR] = {
	gpio_groups, uart_groups, i2c_groups
};

static const struct pinctrl_desc demo_desc = {
	.name = "demo-pinctrl",
	.pins = pins, .npins = NPINS,
	.groups = groups, .ngroups = NGROUPS,
	.function_groups = demo_function_groups, .nfunctions = FUNC_NR,
	.pmxops = &demo_pmxops,
	.confops = &demo_confops,
	.strict = 1,
};

static const struct pinctrl_desc *pctl;		/* 注册进来的控制器 */

static int pinctrl_register(const struct pinctrl_desc *desc)
{
	if (pctl)
		return -1;			/* 一个控制器只能注册一次 */
	if (!desc->pins || !desc->groups || !desc->pmxops)
		return -1;			/* 户口本不全 */
	pctl = desc;
	printf("      [核心] pinctrl_register(\"%s\"): 登记 %u 个 pin / %u 组 / %u 种功能\n",
	       desc->name, desc->npins, desc->ngroups, desc->nfunctions);
	printf("      [核心] 从这一刻起, 别人只能通过 group/function 来找我们要引脚\n");
	return 0;
}

/* 组名 → 组编号 */
static int group_selector(const char *name)
{
	unsigned int i;

	for (i = 0; i < pctl->ngroups; i++)
		if (strcmp(pctl->groups[i].name, name) == 0)
			return (int)i;
	return -1;
}

/* 功能名 → 功能编号 */
static int func_selector(const char *name)
{
	unsigned int i;

	for (i = 0; i < pctl->nfunctions; i++)
		if (strcmp(func_names[i], name) == 0)
			return (int)i;
	return -1;
}

/* 谁已经占着哪一组脚 —— strict 模式下要拦冲突 */
struct claim {
	const char	*group;
	const char	*func;
};
static struct claim claims[8];
static int nclaims;

static struct claim *claim_find(const char *group)
{
	int i;

	for (i = 0; i < nclaims; i++)
		if (strcmp(claims[i].group, group) == 0)
			return &claims[i];
	return NULL;
}

/*
 * 消费者侧的用法(真实内核里就是这两步):
 *      pinctrl_lookup_state(dev, "default")   ← 按设备树里 pinctrl-names 找
 *      pinctrl_select_state(dev, state)       ← 真正去切引脚
 * 设备树里的 pinctrl-0 = <&uart_pins>; 就是这里 consumers[] 的那几行。
 */
struct consumer {
	const char	*dev;		/* 哪个设备 */
	const char	*state;		/* 什么状态(default / sleep / idle) */
	const char	*func;		/* 该状态下这组脚要干什么 */
	const char	*group;		/* 哪一组脚 */
};

static struct consumer consumers[] = {
	{ "uart@0", "default", "uart", "uart-0" },
	{ "uart@0", "sleep",   "gpio", "uart-0" },	/* 休眠时把脚放回 gpio */
	{ "i2c@1",  "default", "i2c",  "uart-0" },	/* 故意写错: 想用别人的组 */
};
#define NCONSUMERS	3

static int pinctrl_select_state(const struct consumer *c)
{
	int fsel, gsel, ret;
	struct claim *cl;

	fsel = func_selector(c->func);
	gsel = group_selector(c->group);
	if (fsel < 0 || gsel < 0) {
		printf("      [核心] 设备树里写的 function/group 不存在, 直接报错\n");
		return -1;
	}

	printf("\n  $ %s 选择状态 \"%s\"  →  要 %s 功能 + %s 组\n",
	       c->dev, c->state, c->func, c->group);

	/* strict: 一组脚同时只能属于一个功能。gpio 例外(它是"没人用的默认态")。 */
	cl = claim_find(c->group);
	if (pctl->strict && cl && strcmp(cl->func, c->func) != 0 && fsel != FUNC_GPIO) {
		printf("      [核心] [拒绝] %s 已经属于 \"%s\"(strict 模式要求先让出来)\n",
		       c->group, cl->func);
		printf("      [核心] 返回 -EBUSY —— 这就是 pinctrl 存在的意义:\n");
		printf("             抢引脚会在申请阶段被拦住, 而不是等到外设莫名其妙不工作。\n");
		return -EBUSY;
	}

	ret = pctl->pmxops->set_mux((unsigned int)fsel, (unsigned int)gsel);
	if (ret)
		return ret;

	if (!cl && nclaims < 8) {
		cl = &claims[nclaims++];
		cl->group = c->group;
	}
	if (cl)
		cl->func = c->func;
	return 0;
}

/* pinconf: 参数打包成一个整数传下去(真实内核就是 unsigned long config) */
static int pinctrl_config_set(unsigned int pin, unsigned int param,
			      unsigned int arg)
{
	if (!pctl || !pctl->confops)
		return -1;
	printf("\n  $ pinconf: PA%u, param=%u, arg=%u\n", pin, param, arg);
	return pctl->confops->pin_config_set(pin, param, arg);
}

/* 打印户口本 —— 内核/用户态看到的信息都来自这张表 */
static void pinctrl_show(void)
{
	unsigned int i, j;

	printf("      [核心] pin controller \"%s\" 的户口本:\n", pctl->name);
	printf("        pins      : ");
	for (i = 0; i < pctl->npins; i++)
		printf("%s ", pctl->pins[i].name);
	printf("\n");

	for (i = 0; i < pctl->ngroups; i++) {
		printf("        group %-8s: ", pctl->groups[i].name);
		for (j = 0; j < pctl->groups[i].npins; j++)
			printf("%s ", pctl->pins[pctl->groups[i].pins[j]].name);
		printf("\n");
	}

	for (i = 0; i < pctl->nfunctions; i++) {
		const char * const *gs = pctl->function_groups[i];

		printf("        func  %-8s: 可挂 ", func_names[i]);
		for (j = 0; gs[j]; j++)
			printf("%s ", gs[j]);
		printf("\n");
	}
}

/* =====================================================================
 *  4. 反面教材: 没有 pinctrl 的世界
 * ===================================================================*/
static void bad_world(void)
{
	printf("\n  ── 对比: 如果两个驱动都直接写寄存器(这就是 pinctrl 出现之前的世界)──\n");
	printf("     串口驱动: PA0.MUX = uart, PA1.MUX = uart\n");
	pin_update(0, MUX_MASK, MUX_SHIFT, FUNC_UART);
	pin_update(1, MUX_MASK, MUX_SHIFT, FUNC_UART);
	printf("     I2C 驱动: 它也想用 PA0(不知道已经被占), PA0.MUX = i2c\n");
	pin_update(0, MUX_MASK, MUX_SHIFT, FUNC_I2C);
	pin_dump("结果");
	printf("     → 串口 TX 突然没波形了, 而且没有任何日志说明是谁干的。\n");
	printf("     → pinctrl 要消灭的就是上面那一行。\n");

	/* 演示完把寄存器清干净 */
	memset(pin_reg, 0, sizeof(pin_reg));
}

int main(void)
{
	puts("==================================================================");
	puts(" pinctrl 原理: pin → group → function, 以及\"谁有权改寄存器\"");
	puts("==================================================================");

	/* ---------- ① 注册 ---------- */
	puts("\n【① 注册 pin controller: 先报户口】");
	if (pinctrl_register(&demo_desc))
		return 1;
	pinctrl_show();

	/* ---------- ② 消费者申请 ---------- */
	puts("\n【② 消费者申请: 核心查表 → 调驱动的 set_mux()】");
	if (pinctrl_select_state(&consumers[0]))
		return 1;
	pin_dump("uart 的 default 状态之后");

	/* ---------- ③ pinconf ---------- */
	puts("\n【③ pinconf: 同样的引脚, 这次改的是电气参数】");
	pinctrl_config_set(0, PARAM_BIAS_PULL_UP, 0);
	pinctrl_config_set(0, PARAM_DRIVE_STRENGTH, 8);
	pinctrl_config_set(0, PARAM_OUTPUT_ENABLE, 1);
	pin_dump("配置完 PA0 之后");
	puts("       (pinmux 决定\"这个脚干什么用\", pinconf 决定\"电特性\", 两件事分开)");

	/* ---------- ④ 冲突 ---------- */
	puts("\n【④ strict 排他: 别人来抢同一组脚】");
	pinctrl_select_state(&consumers[2]);

	/* ---------- ⑤ 让出来 ---------- */
	puts("\n【⑤ 切回 gpio 就等于\"让出来\"】");
	if (pinctrl_select_state(&consumers[1]))
		return 1;
	printf("      (休眠状态把脚放回 gpio: 省电, 也把资源交还给别人)\n");
	pin_dump("sleep 之后");

	/* ---------- ⑥ 反面教材 ---------- */
	bad_world();

	puts("\n==================================================================");
	puts(" 四句话总结");
	puts("   1. 引脚寄存器不能谁都写 —— pinctrl 把写寄存器这件事拆成\"申请 + 裁决\"");
	puts("   2. 单位是 group 不是 pin: 一组脚一起切, 中间不会出现半串口半 I2C");
	puts("   3. pinmux(干什么) 和 pinconf(电特性) 是两条独立的回调");
	puts("   4. 真实内核里的对应关系:");
	puts("        pinctrl_desc / pinmux_ops.set_mux / pinconf_ops.pin_config_set  ← 驱动写的部分");
	puts("        devm_pinctrl_get() + pinctrl_select_state(\"default\")            ← 消费者写的那两行");
	puts("        设备树: pinctrl-names = \"default\"; pinctrl-0 = <&uart_pins>;  ← 就是 consumers[] 这两行");
	puts("==================================================================");
	return 0;
}
