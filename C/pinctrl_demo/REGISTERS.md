# 寄存器说明 —— demo 的「IOMUX」寄存器

这个 demo 把一块假的 IOMUX 寄存器区塞进数组里：

```c
static unsigned int pin_reg[NPINS];   /* 假装这就是 0x40002000 那块寄存器区 */
```

地址映射（`board.dts` 里 `reg = <0x40002000 0x1000>`，每条 4 字节。
`func_demo.c` / `steps_demo.c` 里没有真地址，就是 `pin_reg[]` 这个数组）：

| 引脚 | 地址 | 数组元素 |
|---|---|---|
| PA0 | 0x40002000 | `pin_reg[0]` |
| PA1 | 0x40002004 | `pin_reg[1]` |
| PA2 | 0x40002008 | `pin_reg[2]` |
| PA3 | 0x4000200C | `pin_reg[3]` |

**一个脚一个 32 位寄存器，不是每功能一个。**

---

## 一、位域布局

| bit | 名称 | 宽度 | 含义 |
|---|---|---|---|
| 0–3 | `MUX` | 4 | 复用选择：这个脚接给谁 |
| 4–5 | `PULL` | 2 | 内部上下拉 |
| 6–7 | — | 2 | 保留 |
| 8–11 | `DRIVE` | 4 | 驱动能力，单位 mA |
| 12–15 | — | 4 | 保留 |
| 16 | `OE` | 1 | 输出使能 |
| 17–31 | — | 15 | 保留 |

> `MUX` + `PULL` 正好占满低字节, 所以 `0x0000_0811` 一眼能读出
> 「MUX=1, PULL=1, DRIVE=8」。

对应宏（`func_demo.c` / `steps_demo.c` / `pinctrl_demo.c` / `dts_demo.c` 开头完全一致）：

```c
#define MUX_SHIFT   0    #define MUX_MASK   0xf
#define PULL_SHIFT  4    #define PULL_MASK  0x3
#define DRIVE_SHIFT 8    #define DRIVE_MASK 0xf
#define OE_SHIFT    16                  /* 单 bit，mask 就是 1 */
```

## 二、字段取值编码

**MUX**（4 位，但只有 0–2 有效）

| 值 | 含义 | 宏 |
|---|---|---|
| 0 | gpio（默认态 = 没人用） | `FUNC_GPIO` |
| 1 | uart | `FUNC_UART` |
| 2 | i2c | `FUNC_I2C` |
| 3–15 | 无效，打印时显示 `?` | — |

**PULL**（2 位）

| 值 | 含义 | 宏 |
|---|---|---|
| 0 | 悬空 float | `PULL_FLOAT` |
| 1 | 上拉 | `PULL_UP` |
| 2 | 下拉 | `PULL_DOWN` |
| 3 | 未定义（demo 不用） | — |

**DRIVE**：**不编码**，字段值直接就是 mA 数（0–15 mA → 0x0–0xF）。
真实 SoC 这里通常是「挡位编码」（如 0=2mA, 1=4mA…），需要查芯片手册，demo 简化成 1:1。

**OE**：1 bit，0 = 输入/高阻，1 = 输出使能。

## 三、配置规则（重点）

1. **读-改-写，不能整字覆盖。** 所有写操作都走同一个函数：

   ```c
   pin_reg[pin] &= ~(mask << shift);          /* 清掉目标位段 */
   pin_reg[pin] |=  (val & mask) << shift;    /* 只填这一段 */
   ```

   直接 `pin_reg[pin] = val` 会把 DRIVE、OE 连同保留位一起抹掉 —— 这是驱动里最常见的低级 bug。

2. **写之前先 `& mask` 截断。** 传进来的 `arg` 超出字段宽度时必须被截掉，否则会溢出到相邻字段。

3. **保留位保持原值**（demo 从未写 6–7 / 12–15 / 17–31，所以恒为 0；真实硬件有些保留位必须写 1，得按手册来）。

4. **复位值 = 0**：`MUX=gpio / PULL=悬空 / DRIVE=0mA / OE=0`。
   注意这意味着「没配过的脚」和「被显式配成 gpio 悬空」在寄存器上看起来一样 —— 所以 demo 用软件 `claims[]` 记账，而不是靠读寄存器判断归属。

5. **整组原子写。** `demo_set_mux()` 是 `for` 循环逐脚写，但对消费者呈现为一次操作；
   分成两次调用会出现「PA0 已 uart、PA1 还 gpio」的中间态，对面外设会收垃圾。

6. **不认识的参数必须报错返回**（`default` 分支返回 -1 / 真实内核返回 `-ENOTSUPP`），不能静默忽略。

## 四、pinconf 参数 → 字段映射

参数编号是**照抄内核 `PIN_CONFIG_*` 枚举**的，所以和 `/sys/kernel/debug/pinctrl/*/pinconf-pins` 里看到的数字一致。

| `param` | 内核宏 | 写入字段 | `arg` 用法 |
|---|---|---|---|
| 1 | `PIN_CONFIG_BIAS_DISABLE` | `PULL = 0` | 忽略 |
| 3 | `PIN_CONFIG_BIAS_PULL_DOWN` | `PULL = 2` | 忽略 |
| 5 | `PIN_CONFIG_BIAS_PULL_UP` | `PULL = 1` | 忽略 |
| 9 | `PIN_CONFIG_DRIVE_STRENGTH` | `DRIVE = arg` | mA 数 |
| 18 | `PIN_CONFIG_OUTPUT_ENABLE` | `OE = arg?1:0` | 0/1 |

> MUX 不在 pinconf 里 —— 它属于 **pinmux**，由 `set_mux(func, group)` 整组写。

## 五、设备树属性 → 字段

`board.dts` 里的属性最终就变成上表的调用：

| dts 属性 | 效果 |
|---|---|
| `function = "uart"` | 整组 `MUX = 1` |
| `pins = "PA0","PA1"` | 决定 `pin` 编号（组内逐脚） |
| `bias-pull-up` | `PULL = 1` |
| `bias-pull-down` | `PULL = 2` |
| `bias-disable` | `PULL = 0` |
| `drive-strength = <8>` | `DRIVE = 8` |
| `output-enable` | `OE = 1` |

## 六、算个值验证一下

PA0 配成 uart + 上拉 + 8mA + 输出使能：

| 字段 | 值 | 移位后 |
|---|---|---|
| MUX | 1 | `0x00000001` |
| PULL | 1 | `0x00000010` |
| DRIVE | 8 | `0x00000800` |
| OE | 1 | `0x00010000` |
| — | — | **`0x00010811`** |

和 `pin_dump()` 打印的 `(0x........)` 对照即可。只看 `make run` / `make dts` 的输出就知道每一步改的是哪几个 bit。

## 七、func_demo.c 里每个 demo 碰了哪些位

| demo | 碰寄存器? | 动的字段/位 |
|---|---|---|
| ① `demo_pin_update` | ✅ 写 | 依次单独写 MUX → PULL → DRIVE → OE；第 ⑤ 步演示 `0x1f & 0xf` 截断；第 ⑥ 步只把 MUX 改回 0 |
| ② `demo_top_bit` | ❌ | 只算 mask 的位宽（`0xf→3`、`0x3→1`），用来打印 `bit3:0` 这种范围 |
| ③ `demo_func_lookup` | ❌ | 功能名 → MUX 编号，查不到返回 `-1` |
| ④ `demo_group_lookup` | ❌ | 组名 → 组（含脚号），查不到返回 `NULL` |
| ⑤ `demo_apply` | ✅ 写 | 调 `pin_update` 后**逐位比对** before/after，只报真正翻转的 bit；值没变就打印「无」 |
| ⑥ `demo_dump_all` | ❌ 只读 | 按位移位解码 4 个字段，`MUX>2` 显示 `?` |
| ⑦ `demo_direct_write`（反面） | ✅ 整字写 | `pin_reg[0] = 1` 把 PULL/DRIVE/OE 连同保留位全抹掉，`0x00010811 → 0x00000001` |

一句话：**写寄存器只有 `pin_update` 一处**，其余函数不是查表就是只读打印。

## 八、和真实硬件的差距

- 真实 IOMUX 一般是 **每脚 4 字节 + 每功能另有独立寄存器**（有的 SoC MUX 按 bit 位「置 1 选功能」而不是编号），布局必须照手册。
- 真实寄存器区要走 `ioremap`/`regmap`，不能用数组；写前常有 `readl` 回读校验。
- `DRIVE` 一般是编码挡位而非 mA 原值；`PULL` 有的芯片是「上下拉各 1 bit」而非 2 bit 编码。
- 真实 pinconf 的参数更多（施密特触发、压摆率、去抖、低功耗态等），demo 只挑 5 个。
