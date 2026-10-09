/* soc_sim.c —— 主机上的"虚拟硬件"（只有 PC 版会编这个文件）
 *
 * 芯片上，写 0x40000000 这些地址会真的落到电路上；
 * PC 上没有这段电路，所以我们做两件事：
 *
 *   1. 用 mmap 把 0x40000000 起 256KB 真的映射成一段普通内存 —— 地址合法了
 *   2. 每次读写都顺手"演"一下外设该有的反应 —— 于是它像个芯片了
 *      （硬件真实的做法叫 MMIO 总线译码；QEMU 这类模拟器用的也是这一招）
 *
 * 这一步是"同一份固件代码能在 PC 上跑"的关键：
 * 驱动代码照常写寄存器，只是 REG_W/REG_R 宏被换成了走这里的函数。
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/mman.h>

#include "me1000.h"
#include "soc_sim.h"

static volatile uint32_t *g_regs;                                  /* 寄存器区 */
static uint32_t           g_shadow[PERIPH_SIZE / sizeof(uint32_t)]; /* 映射失败时的退路 */

static uint32_t g_led_level = 0xFFFFFFFFu;   /* 0xFFFFFFFF 表示"还不知道"，保证第一次会打印 */
static int      g_led_is_output;
static int      g_uart_up;

static volatile uint32_t *reg_of(uintptr_t addr)
{
    return &g_regs[(addr - PERIPH_BASE) / sizeof(uint32_t)];
}

void soc_sim_init(void)
{
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef MAP_FIXED_NOREPLACE
    flags |= MAP_FIXED_NOREPLACE;       /* 就要这个地址，但别覆盖已有映射 */
#endif
    void *p = mmap((void *)(uintptr_t)PERIPH_BASE, PERIPH_SIZE,
                   PROT_READ | PROT_WRITE, flags, -1, 0);

    if (p == MAP_FAILED) {
        /* 地址真的被占了也没关系：反正所有访问都经过 g_regs 寻址 */
        g_regs = g_shadow;
        printf("  [主机]   固定地址映射失败，改用一块普通数组当寄存器区（行为完全一样）\n");
        return;
    }
    g_regs = (volatile uint32_t *)p;
    printf("  [主机]   外设寄存器区 0x%08x 起 %uKB 已映射成普通内存\n",
           (unsigned)PERIPH_BASE, (unsigned)(PERIPH_SIZE / 1024u));
}

/* ------------------------------------------------------------------
 * 外设的"反应"：这一段就是芯片的手册，只不过用 printf 写成好懂的版本
 *
 * 重点看 RCC 那三条：软件只负责"请求"，"完成"永远是硬件回置的状态位。
 * 所以驱动里必须写"置位 -> 轮询 RDY"，绝不能写完就当成功了。
 * ------------------------------------------------------------------ */
static void sim_react(uintptr_t addr, uint32_t val)
{
    if (addr == RCC_BASE + RCC_CR) {
        uint32_t already = *reg_of(addr);   /* 只看"这次是不是新请求"，避免重复播报 */
        if ((val & RCC_CR_HSEON) != 0u && (already & RCC_CR_HSERDY) == 0u) {
            *reg_of(addr) = val | RCC_CR_HSERDY;
            printf("  [虚拟硬件] 看到 HSEON=1 -> 回置 HSERDY：晶振起振了\n");
        }
        if ((val & RCC_CR_PLLON) != 0u && (already & RCC_CR_PLLRDY) == 0u) {
            *reg_of(addr) |= RCC_CR_PLLRDY;
            printf("  [虚拟硬件] 看到 PLLON=1 -> 回置 PLLRDY：PLL 锁定了\n");
        }
    } else if (addr == RCC_BASE + RCC_CFGR) {
        uint32_t sw = (val & RCC_CFGR_SW_MASK) >> RCC_CFGR_SW_POS;
        uint32_t sws = (*reg_of(addr) & RCC_CFGR_SWS_MASK) >> RCC_CFGR_SWS_POS;
        if (sw == RCC_CFGR_SW_PLL && sws != RCC_CFGR_SW_PLL) {
            *reg_of(addr) = (val & ~RCC_CFGR_SWS_MASK)
                            | (RCC_CFGR_SW_PLL << RCC_CFGR_SWS_POS);
            printf("  [虚拟硬件] 看到 SW=PLL -> 回置 SWS=PLL：CPU 心跳已切到 168MHz\n");
        }
    } else if (addr == LED_PORT + GPIO_MODER) {
        uint32_t mode = (val >> (LED_PIN * 2u)) & 3u;
        if (mode == GPIO_MODE_OUTPUT && g_led_is_output == 0) {
            g_led_is_output = 1;
            printf("  [虚拟硬件] %s 的方向位 = 输出，引脚交给我们控制了\n", LED_PIN_NAME);
        }
    } else if (addr == LED_PORT + GPIO_ODR) {
        uint32_t level = (val >> LED_PIN) & 1u;
        if (level != g_led_level) {
            g_led_level = level;
            printf("  [虚拟硬件] %s 输出 %u -> 板上 LED %s\n",
                   LED_PIN_NAME, (unsigned)level, (level != 0u) ? "亮" : "灭");
        }
    } else if (addr == UART0_BASE + USART_CR1) {
        if ((val & USART_CR1_UE) != 0u && g_uart_up == 0) {
            g_uart_up = 1;
            printf("  [虚拟硬件] UART 使能 (CR1.UE=1)，BRR=%u；"
                   "之后每往 DR 写一个字节，我就在这里显示一个字符：\n",
                   (unsigned)*reg_of(UART0_BASE + USART_BRR));
        }
    } else if (addr == UART0_BASE + USART_DR) {
        fputc((int)(val & 0xFFu), stdout);      /* 这就是主机上的"串口线" */
    }
}

void sim_mmio_write(uintptr_t addr, uint32_t val)
{
    *reg_of(addr) = val;        /* 先老老实实存下来（回读才有意义） */
    sim_react(addr, val);       /* 再让"外设"做出反应 */
}

uint32_t sim_mmio_read(uintptr_t addr)
{
    /* 读 UART 状态寄存器：硬件永远告诉我们"可以发下一个字节了"，
     * 否则驱动里的等待循环会永远转下去。 */
    if (addr == UART0_BASE + USART_SR) {
        *reg_of(addr) = USART_SR_TXE | USART_SR_TC;
    }
    return *reg_of(addr);
}
