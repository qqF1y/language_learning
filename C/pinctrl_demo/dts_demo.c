/*
 * dts_demo.c —— 设备树是怎么变成"引脚配置"的
 *
 * 上一个 demo(pinctrl_demo.c)把板级信息写死在 C 代码里; 这个 demo 把它全挪进
 * board.dts, 然后走一遍内核真实的那条路:
 *
 *      bootloader 交来 dtb
 *            ↓ 内核解析成一棵属性树        ← 本文件第 1 段: 极简 dts 解析器
 *      设备树里的节点
 *            ↓ 内核给 compatible 找驱动     ← 第 2 段: driver 匹配
 *      驱动的 probe() 被调用
 *            ↓ devm_pinctrl_get(dev)       ← 第 3 段: 读 pinctrl-names
 *            ↓ pinctrl_lookup_state("default")
 *            ↓ pinctrl_select_state(state) ← 解析 pinctrl-0 = <&uart_pins>
 *      调 pin controller 的 set_mux()/pin_config_set()
 *            ↓
 *        寄存器变了, 引脚能用了
 *
 * 用法:  ./dts_demo board.dts        (正常)
 *        ./dts_demo board-bad.dts    (label 打错一个字母会怎样)
 *
 * 想自己玩: 直接改 board.dts 里的 pins / function / bias-pull-up, 再跑一次。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =====================================================================
 *  0. 硬件: 引脚寄存器(和 pinctrl_demo.c 里那套一样)
 * ===================================================================*/
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

/* 功能名 → 写进 MUX 的编号。这是**芯片手册里的知识**, 不是设备树里的 ——
 * 设备树只说"这组脚要用 uart", 到底填哪个数由驱动决定。 */
static const char * const func_names[] = { "gpio", "uart", "i2c" };
#define FUNC_NR		3

/* pinconf 的通用参数编号(和内核 enum pin_config_param 一致) */
#define PARAM_BIAS_DISABLE	1
#define PARAM_BIAS_PULL_DOWN	3
#define PARAM_BIAS_PULL_UP	5
#define PARAM_DRIVE_STRENGTH	9
#define PARAM_OUTPUT_ENABLE	18

static unsigned int pin_reg[NPINS];

static const char * const pin_names[NPINS] = { "PA0", "PA1", "PA2", "PA3" };

static int pin_num(const char *name)
{
	int i;

	for (i = 0; i < NPINS; i++)
		if (strcmp(pin_names[i], name) == 0)
			return i;
	return -1;
}

static int func_num(const char *name)
{
	int i;

	for (i = 0; i < FUNC_NR; i++)
		if (strcmp(func_names[i], name) == 0)
			return i;
	return -1;
}

static void pin_update(unsigned int pin, unsigned int mask, unsigned int shift,
		       unsigned int val)
{
	pin_reg[pin] &= ~(mask << shift);
	pin_reg[pin] |= (val & mask) << shift;
}

static void reg_dump(const char *tag)
{
	unsigned int i;

	printf("      [寄存器] %s\n", tag);
	for (i = 0; i < NPINS; i++) {
		unsigned int r = pin_reg[i];
		unsigned int mux = (r >> MUX_SHIFT) & MUX_MASK;

		printf("        %s: MUX=%-4s PULL=%-4s DRIVE=%2umA OE=%u   (0x%08x)\n",
		       pin_names[i], mux < FUNC_NR ? func_names[mux] : "?",
		       (const char *[]) { "悬空", "上拉", "下拉" }[(r >> PULL_SHIFT) & PULL_MASK],
		       (r >> DRIVE_SHIFT) & DRIVE_MASK, (r >> OE_SHIFT) & 1, r);
	}
}

/* =====================================================================
 *  1. 极简 dts 解析器 —— 内核那棵"属性树"就是这么来的
 *
 *  只认 demo 用到的语法: 节点 { ... }、label: name {、key = value;、key;
 *  值原样存下来, 用的时候再解析(内核其实也是这样: dtb 里也是原始数据)。
 * ===================================================================*/
#define MAX_NODES	48
#define MAX_PROPS	10

struct dt_prop {
	char key[24];
	char val[128];
};

struct dt_node {
	char		 name[32];	/* 节点名, 如 pinctrl@40002000 */
	char		 path[96];	/* 全路径, 如 /pinctrl@40002000/uart-pins */
	char		 label[24];	/* 标签, 如 uart_pins(没有就是空串) */
	struct dt_prop	 props[MAX_PROPS];
	int		 nprops;
	int		 parent;	/* 父节点下标, 根是 -1 */
};

static struct dt_node dt[MAX_NODES];
static int dt_nodes;
static int in_block_comment;

/* 去掉 // 行注释和块注释(块注释可能跨行, 所以要记住状态) */
static void strip_comments(char *s)
{
	char out[256];
	int i = 0, j = 0;

	while (s[i]) {
		if (in_block_comment) {
			if (s[i] == '*' && s[i + 1] == '/') {
				in_block_comment = 0;
				i += 2;
			} else {
				i++;
			}
			continue;
		}
		if (s[i] == '/' && s[i + 1] == '*') {
			in_block_comment = 1;
			i += 2;
			continue;
		}
		if (s[i] == '/' && s[i + 1] == '/')
			break;
		out[j++] = s[i++];
	}
	out[j] = '\0';
	strcpy(s, out);
}

static char *trim(char *s)
{
	char *end;

	while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		s++;
	end = s + strlen(s);
	while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
			   end[-1] == '\r' || end[-1] == '\n'))
		*--end = '\0';
	return s;
}

static int dt_add_node(const char *name, const char *label, int parent)
{
	struct dt_node *n;

	if (dt_nodes >= MAX_NODES)
		return -1;
	n = &dt[dt_nodes];
	memset(n, 0, sizeof(*n));
	snprintf(n->name, sizeof(n->name), "%s", name);
	snprintf(n->label, sizeof(n->label), "%s", label ? label : "");
	n->parent = parent;

	if (parent < 0)
		snprintf(n->path, sizeof(n->path), "/");
	else if (strcmp(dt[parent].path, "/") == 0)
		snprintf(n->path, sizeof(n->path), "/%s", name);
	else
		snprintf(n->path, sizeof(n->path), "%s/%s", dt[parent].path, name);

	return dt_nodes++;
}

static void dt_add_prop(struct dt_node *n, char *text)
{
	char *eq = strchr(text, '=');
	char *key, *val, *semi;

	if (n->nprops >= MAX_PROPS)
		return;

	if (eq) {
		*eq = '\0';
		key = trim(text);
		val = trim(eq + 1);
	} else {
		key = trim(text);
		val = (char *)"";
	}
	semi = strrchr(key, ';');		/* 无值属性: "output-enable;" */
	if (semi)
		*semi = '\0';
	key = trim(key);
	semi = strrchr(val, ';');
	if (semi)
		*semi = '\0';
	val = trim(val);

	snprintf(n->props[n->nprops].key, sizeof(n->props[0].key), "%s", key);
	snprintf(n->props[n->nprops].val, sizeof(n->props[0].val), "%s", val);
	n->nprops++;
}

static int parse_dts(const char *file)
{
	FILE *f = fopen(file, "r");
	char line[256];
	int stack[MAX_NODES], sp = 0;

	if (!f) {
		printf("  !! 打不开 %s\n", file);
		return -1;
	}

	dt_nodes = 0;
	in_block_comment = 0;
	stack[sp++] = dt_add_node("/", "", -1);		/* 根节点 */

	while (fgets(line, sizeof(line), f)) {
		char *p, *brace, *colon;
		int parent;

		strip_comments(line);
		p = trim(line);
		if (!*p || strcmp(p, "/dts-v1/;") == 0)
			continue;

		if (*p == '}') {			/* 节点结束 */
			if (sp > 1)
				sp--;
			continue;
		}

		brace = strchr(p, '{');
		if (brace) {				/* 节点开始 */
			char name[32] = "", label[24] = "";

			*brace = '\0';
			p = trim(p);
			if (strcmp(p, "/") == 0)
				continue;	/* "/ {" 就是根节点, 已经建过了 */
			colon = strchr(p, ':');
			if (colon) {
				*colon = '\0';
				snprintf(label, sizeof(label), "%s", trim(p));
				p = trim(colon + 1);
			}
			snprintf(name, sizeof(name), "%s", p);

			parent = stack[sp - 1];
			if (dt_add_node(name, label, parent) >= 0 && sp < MAX_NODES)
				stack[sp++] = dt_nodes - 1;
			continue;
		}

		dt_add_prop(&dt[stack[sp - 1]], p);	/* 普通属性 */
	}
	fclose(f);
	return 0;
}

/* ---------- 读属性: 相当于内核的 of_property_* ---------- */
static const char *dt_prop(const struct dt_node *n, const char *key)
{
	int i;

	for (i = 0; i < n->nprops; i++)
		if (strcmp(n->props[i].key, key) == 0)
			return n->props[i].val;
	return NULL;
}

/*
 * 把值里的 token 抠出来:
 *     "PA0", "PA1"      → PA0 / PA1
 *     <&uart_pins>      → uart_pins      (去掉 & 和尖括号)
 *     "default", "sleep"→ default / sleep
 * 真实内核里这是 of_property_read_string_array / for_each_of_phandle_args。
 */
static int dt_tokens(const char *val, char out[][24], int max)
{
	int n = 0;
	const char *p = val;

	if (!p)
		return 0;

	while (*p && n < max) {
		if (*p == '"') {
			const char *e = strchr(p + 1, '"');
			size_t len;

			if (!e)
				break;
			len = (size_t)(e - p - 1);
			if (len > 23)
				len = 23;
			memcpy(out[n], p + 1, len);
			out[n][len] = '\0';
			n++;
			p = e + 1;
		} else if (*p == '<') {
			const char *e = strchr(p + 1, '>');

			if (!e)
				break;
			p++;
			while (p < e && n < max) {	/* 尖括号里可能有好几个, 空格分开 */
				char tok[24];
				size_t len = 0;

				while (p < e && (*p == ' ' || *p == '\t' || *p == '\n'))
					p++;
				while (p < e && *p != ' ' && *p != '\t' && *p != '\n' &&
				       len < 23)
					tok[len++] = *p++;
				if (!len)
					break;
				tok[len] = '\0';
				if (tok[0] == '&')		/* phandle 引用 */
					memmove(tok, tok + 1, len);
				snprintf(out[n], 24, "%s", tok);
				n++;
			}
			p = e + 1;
		} else {
			p++;
		}
	}
	return n;
}

static struct dt_node *dt_by_label(const char *label)
{
	int i;

	for (i = 0; i < dt_nodes; i++)
		if (strcmp(dt[i].label, label) == 0)
			return &dt[i];
	return NULL;
}

/* 读一个字符串属性(值里的引号要去掉): of_property_read_string 的简化版 */
static const char *dt_prop_str(const struct dt_node *n, const char *key,
			       char *buf, size_t size)
{
	char tokens[1][24];

	if (dt_tokens(dt_prop(n, key), tokens, 1) < 1)
		return NULL;
	snprintf(buf, size, "%s", tokens[0]);
	return buf;
}

static struct dt_node *dt_by_compatible(const char *compat)
{
	int i;

	for (i = 0; i < dt_nodes; i++)
		if (dt[i].parent >= 0 && dt_prop(&dt[i], "compatible") &&
		    strstr(dt_prop(&dt[i], "compatible"), compat))
			return &dt[i];
	return NULL;
}

static void dt_dump(void)
{
	int i, j;

	puts("\n【一】内核拿到 dtb 之后, 看到的是这样一棵属性树");
	for (i = 0; i < dt_nodes; i++) {
		int depth = 0, p = dt[i].parent;

		while (p >= 0) {
			depth++;
			p = dt[p].parent;
		}
		if (depth == 0) {
			printf("      /\n");		/* 根节点: 只印属性, 不缩进 */
		} else {
			printf("      %*s%s", (depth - 1) * 2, "", dt[i].name);
			if (dt[i].label[0])
				printf("   [标签 %s]", dt[i].label);
			putchar('\n');
		}

		for (j = 0; j < dt[i].nprops; j++)
			printf("      %*s  %s = %s\n", (depth - 1) * 2, "",
			       dt[i].props[j].key,
			       dt[i].props[j].val[0] ? dt[i].props[j].val
						     : "(true)");
	}
}

/* =====================================================================
 *  2. pin controller: 从设备树里"发现"有哪几组脚
 *     —— 注意: 组名/功能/引脚全来自 dts, 驱动里一个都没写死
 * ===================================================================*/
struct group {
	const char	*name;
	char		 function[16];	/* 去引号后的功能名, 如 uart */
	unsigned int	 pins[NPINS];
	unsigned int	 npins;
	struct dt_node	*node;
};

static struct group groups[8];
static int n_groups;

static int pinctrl_scan_groups(struct dt_node *pctl)
{
	int i, pctl_idx = (int)(pctl - dt);
	int g;

	n_groups = 0;
	for (i = 0; i < dt_nodes; i++) {
		char tokens[NPINS][24];
		int n, k;

		if (dt[i].parent != pctl_idx)
			continue;
		if (n_groups >= 8)
			break;

		g = n_groups++;
		groups[g].node = &dt[i];
		groups[g].name = dt[i].name;
		groups[g].npins = 0;
		if (!dt_prop_str(&dt[i], "function", groups[g].function,
				 sizeof(groups[g].function)))
			snprintf(groups[g].function, sizeof(groups[g].function), "%s", "?");

		n = dt_tokens(dt_prop(&dt[i], "pins"), tokens, NPINS);
		for (k = 0; k < n; k++) {
			int num = pin_num(tokens[k]);

			if (num >= 0)
				groups[g].pins[groups[g].npins++] = (unsigned int)num;
		}
	}

	printf("      [核心] 从设备树的 pinctrl 节点下面发现 %d 组脚:\n", n_groups);
	for (g = 0; g < n_groups; g++) {
		printf("             %-18s function=%-5s pins=", groups[g].name,
		       groups[g].function);
		for (i = 0; i < (int)groups[g].npins; i++)
			printf("%s ", pin_names[groups[g].pins[i]]);
		printf("\n");
	}
	return n_groups;
}

/* 驱动侧: 把一组脚切到某个功能(整组一起切) */
static int set_mux(const struct group *g)
{
	int f = func_num(g->function);
	unsigned int i;

	if (f < 0) {
		printf("             !! 设备树里的 function=\"%s\" 驱动不认识\n",
		       g->function);
		return -1;
	}

	printf("      [pin控制器] set_mux(%s, %s)\n", g->function, g->name);
	for (i = 0; i < g->npins; i++)
		pin_update(g->pins[i], MUX_MASK, MUX_SHIFT, (unsigned int)f);
	return 0;
}

/* 驱动侧: 把 dts 里那几条电气参数变成 PIN_CONFIG_* 再配置 ——
 * 真实内核里这步叫 pinconf_generic_dt_node_to_map()。 */
static int apply_pinconf_from_dt(const struct group *g)
{
	static const struct {
		const char	*prop;		/* dts 里的属性名 */
		unsigned int	 param;		/* 内核通用参数编号 */
		int		 has_arg;
	} table[] = {
		{ "bias-disable",	PARAM_BIAS_DISABLE,	0 },
		{ "bias-pull-up",	PARAM_BIAS_PULL_UP,	0 },
		{ "bias-pull-down",	PARAM_BIAS_PULL_DOWN,	0 },
		{ "drive-strength",	PARAM_DRIVE_STRENGTH,	1 },
		{ "output-enable",	PARAM_OUTPUT_ENABLE,	1 },
	};
	unsigned int i, k;

	for (i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
		const char *val = dt_prop(g->node, table[i].prop);
		unsigned int arg = 1;
		char tokens[1][24];

		if (!val)
			continue;
		if (table[i].has_arg) {
			if (dt_tokens(val, tokens, 1) < 1)
				continue;
			arg = (unsigned int)strtoul(tokens[0], NULL, 0);
		}

		for (k = 0; k < g->npins; k++) {
			unsigned int pin = g->pins[k];

			switch (table[i].param) {
			case PARAM_BIAS_DISABLE:
				pin_update(pin, PULL_MASK, PULL_SHIFT, PULL_FLOAT);
				break;
			case PARAM_BIAS_PULL_UP:
				pin_update(pin, PULL_MASK, PULL_SHIFT, PULL_UP);
				break;
			case PARAM_BIAS_PULL_DOWN:
				pin_update(pin, PULL_MASK, PULL_SHIFT, PULL_DOWN);
				break;
			case PARAM_DRIVE_STRENGTH:
				pin_update(pin, DRIVE_MASK, DRIVE_SHIFT, arg);
				break;
			case PARAM_OUTPUT_ENABLE:
				pin_update(pin, 1, OE_SHIFT, arg ? 1 : 0);
				break;
			default:
				break;
			}
		}
		printf("      [pin控制器] pinconf: %s (=%u) 应用到", table[i].prop,
		       table[i].param);
		for (k = 0; k < g->npins; k++)
			printf(" %s", pin_names[g->pins[k]]);
		printf("\n");
	}
	return 0;
}

/* =====================================================================
 *  3. 消费者侧: pinctrl-names / pinctrl-N 是怎么被用掉的
 * ===================================================================*/
static int pinctrl_select_state(struct dt_node *consumer, const char *state)
{
	char names[4][24];
	char phandles[4][24];
	char key[24];
	int n_names, n_ph, i, k, idx = -1;

	/* ① "sleep" 是第几个名字? → 就去找 pinctrl-<那个数字> */
	n_names = dt_tokens(dt_prop(consumer, "pinctrl-names"), names, 4);
	for (i = 0; i < n_names; i++)
		if (strcmp(names[i], state) == 0)
			idx = i;
	if (idx < 0) {
		printf("      [核心] 设备树里没有叫 \"%s\" 的 state(pinctrl-names 里没有)\n",
		       state);
		return -1;
	}

	snprintf(key, sizeof(key), "pinctrl-%d", idx);
	n_ph = dt_tokens(dt_prop(consumer, key), phandles, 4);
	printf("      [核心] state=\"%s\" → pinctrl-names 里排第 %d → 读属性 %s = %s\n",
	       state, idx, key, dt_prop(consumer, key) ? dt_prop(consumer, key) : "");

	/* ② 每个 phandle 指向 pin controller 下的一个 group 节点 */
	for (i = 0; i < n_ph; i++) {
		struct dt_node *gn = dt_by_label(phandles[i]);
		struct group *g = NULL;

		if (!gn) {
			printf("      [核心] !! 找不到标签 \"%s\" 对应的节点 —— 设备树写错了\n",
			       phandles[i]);
			printf("             (现象: 驱动照样加载、设备节点也在, 就是引脚没配上)\n");
			return -1;
		}
		for (k = 0; k < n_groups; k++)
			if (groups[k].node == gn)
				g = &groups[k];
		if (!g) {
			printf("      [核心] !! 标签 \"%s\" 不在 pin controller 下面\n",
			       phandles[i]);
			return -1;
		}

		/* ③ 真正干活: 切复用 + 配电气参数 */
		if (set_mux(g))
			return -1;
		apply_pinconf_from_dt(g);
	}
	return 0;
}

/* =====================================================================
 *  4. 驱动模型: 内核按 compatible 找驱动 → 调 probe
 * ===================================================================*/
struct driver {
	const char	*compatible;
	const char	*name;
	int		(*probe)(struct dt_node *node);
};

static int uart_probe(struct dt_node *node)
{
	char compat[24];

	dt_prop_str(node, "compatible", compat, sizeof(compat));
	printf("\n  [内核] 节点 %s 的 compatible = \"%s\" → 匹配到驱动 demo-uart\n",
	       node->name, compat);
	printf("  [驱动] uart_probe() 开始工作\n");
	printf("  [驱动] p = devm_pinctrl_get(&pdev->dev)      // 读设备树里的 pinctrl-* 属性\n");
	printf("  [驱动] st = pinctrl_lookup_state(p, \"default\")\n");
	if (pinctrl_select_state(node, "default"))
		return -1;

	printf("  [驱动] 引脚配好了, 可以初始化寄存器、注册 tty 了\n");
	return 0;
}

static int i2c_probe(struct dt_node *node)
{
	char compat[24];

	dt_prop_str(node, "compatible", compat, sizeof(compat));
	printf("\n  [内核] 节点 %s 的 compatible = \"%s\" → 匹配到驱动 demo-i2c\n",
	       node->name, compat);
	printf("  [驱动] i2c_probe() 开始工作\n");
	if (pinctrl_select_state(node, "default"))
		return -1;
	return 0;
}

static const struct driver drivers[] = {
	{ "demo,uart", "demo-uart", uart_probe },
	{ "demo,i2c",  "demo-i2c",  i2c_probe  },
};
#define NDRIVERS	(sizeof(drivers) / sizeof(drivers[0]))

int main(int argc, char **argv)
{
	const char *file = argc > 1 ? argv[1] : "board.dts";
	struct dt_node *pctl, *uart;
	unsigned int i;

	printf("==================================================================\n");
	printf(" 设备树 → 引脚配置:  %s\n", file);
	printf("==================================================================\n");

	if (parse_dts(file))
		return 1;

	dt_dump();

	puts("\n【二】内核先找到引脚控制器(pin controller)");
	pctl = dt_by_compatible("demo,pinctrl");
	if (!pctl) {
		puts("      !! 设备树里没有 compatible = \"demo,pinctrl\" 的节点");
		return 1;
	}
	printf("      %s  (compatible = %s, 寄存器 %s)\n", pctl->name,
	       dt_prop(pctl, "compatible"), dt_prop(pctl, "reg"));
	pinctrl_scan_groups(pctl);

	puts("\n【三】逐个设备 probe: compatible 匹配 → 驱动起来 → 顺手把引脚配了");
	for (i = 0; i < NDRIVERS; i++) {
		struct dt_node *n = dt_by_compatible(drivers[i].compatible);

		if (!n) {
			printf("      (没有 %s 的设备)\n", drivers[i].compatible);
			continue;
		}
		if (drivers[i].probe(n)) {
			printf("  [内核] %s 的 probe 失败 → 这个设备起不来\n", n->name);
			printf("         dmesg 里只会留下一行: \"probe of %s failed with error -19\"\n",
			       n->name);
			reg_dump("失败之后: 引脚全都没配(和复位值一样)");
			return 1;
		}
	}

	reg_dump("probe 完之后");

	/* ---------- 运行中切状态 ---------- */
	puts("\n【四】运行中切到 sleep: 驱动调 pinctrl_select_state(p, \"sleep\")");
	uart = dt_by_compatible("demo,uart");
	if (uart) {
		printf("  [驱动] 系统要休眠了, 把串口的脚放回 gpio(省电 + 让给别人)\n");
		if (pinctrl_select_state(uart, "sleep"))
			return 1;
		reg_dump("sleep 之后");
	}

	puts("\n【五】回头看: 这一切都只是\"读了一个文件\"");
	puts("      组名/引脚/功能/上下拉/驱动能力 —— 全在 board.dts 里, 驱动里一行都没有。");
	puts("      换块板子只改 dts; 改 dts 不需要重新编译驱动, 更不需要改驱动源码。");
	puts("      自己试试: 把 board.dts 里 uart-pins 的 pins 改成 \"PA2\", \"PA3\", 再跑一次。");
	return 0;
}
