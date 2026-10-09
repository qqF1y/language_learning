/* system_me1000.c —— 芯片级初始化：把心跳从 16MHz 提到 168MHz
 *
 * 这一层只干"和芯片本身有关、和具体应用无关"的事，所以 main.c 里看不到它 ——
 * 是启动文件在 main 之前调 SystemInit() 干的。这也是为什么很多工程
 * main 一进去时钟就已经是好的。
 *
 * 所有 SoC 的时钟配置都是这三步，换个芯片只是数字和寄存器名不同：
 *
 *   1. 开源头   ：外部晶振上电后是"睡着"的，先写 HSEON，再等硬件回置 HSERDY
 *   2. 配 PLL   ：倍频器把 8MHz 抬到 168MHz，同样要等 PLLRDY
 *   3. 切开关   ：把 CPU 的心跳从 HSI 切到 PLL，回读 SWS 确认切成功了
 *
 * "请求 -> 等 RDY -> 回读确认" 这三拍，是嵌入式里最基本的安全姿势：
 * 硬件比软件慢，软件必须问它"好了没"，不能想当然。
 */
#include "system_me1000.h"
#include "me1000.h"

#define HSE_HZ      8000000u    /* 板子上的晶振频率 */
#define PLLM        8u          /* 8MHz / 8  = 1MHz  （PLL 的输入要 1~2MHz 最好锁） */
#define PLLN        336u        /* 1MHz * 336 = 336MHz                  */
#define PLLP        2u          /* 336MHz / 2 = 168MHz = SYSCLK         */

#define WAIT_LIMIT  1000000u    /* 超时上限：宁可停下来报错，也不要无声地卡死 */

/* 全局唯一的"心跳真值"：上电默认走内部 RC(HSI)，先按 16MHz 记账。
 * 它住在 .data 段（有初值），能反过来验证启动代码有没有把 .data 搬好。 */
uint32_t SystemCoreClock = 16000000u;

/* 忙等某一位变成 1。返回 0 = 成功，-1 = 超时 */
static int wait_ready(uintptr_t reg, uint32_t mask)
{
    for (uint32_t i = 0; i < WAIT_LIMIT; i++) {
        if ((REG_R(reg) & mask) != 0u) {
            return 0;
        }
    }
    return -1;
}

void SystemInit(void)
{
    /* ---------- 第 1 步：开外部晶振 ---------- */
    /* 注意是"置位"而不是"赋值"：CR 里还住着别的开关，别一把抹掉 */
    REG_SET_BITS(RCC_BASE + RCC_CR, RCC_CR_HSEON);
    if (wait_ready(RCC_BASE + RCC_CR, RCC_CR_HSERDY) != 0) {
        for (;;) { }        /* 晶振都没起振，后面的一切都是错的，就地停住最安全 */
    }

    /* ---------- 第 2 步：配 PLL ---------- */
    /* SYSCLK = HSE / PLLM * PLLN / PLLP = 8MHz / 8 * 336 / 2 = 168MHz */
    REG_MODIFY(RCC_BASE + RCC_PLLCFGR, 0x3Fu << RCC_PLLCFGR_PLLM_POS,
               PLLM << RCC_PLLCFGR_PLLM_POS);
    REG_MODIFY(RCC_BASE + RCC_PLLCFGR, 0x1FFu << RCC_PLLCFGR_PLLN_POS,
               PLLN << RCC_PLLCFGR_PLLN_POS);
    REG_MODIFY(RCC_BASE + RCC_PLLCFGR, 0x3u << RCC_PLLCFGR_PLLP_POS, 0u);  /* 0 表示 /2 */

    REG_SET_BITS(RCC_BASE + RCC_CR, RCC_CR_PLLON);
    if (wait_ready(RCC_BASE + RCC_CR, RCC_CR_PLLRDY) != 0) {
        for (;;) { }
    }

    /* ---------- 第 3 步：把 CPU 接到 PLL 上 ---------- */
    /* SW  = 我要切到谁（写）      SWS = 现在实际是谁（读）
     * 这两个位名字很像，但一个是"愿望"、一个是"事实"，读错了会莫名其妙。 */
    REG_MODIFY(RCC_BASE + RCC_CFGR, RCC_CFGR_SW_MASK,
               RCC_CFGR_SW_PLL << RCC_CFGR_SW_POS);

    for (uint32_t i = 0; i < WAIT_LIMIT; i++) {
        uint32_t sws = (REG_R(RCC_BASE + RCC_CFGR) & RCC_CFGR_SWS_MASK)
                       >> RCC_CFGR_SWS_POS;
        if (sws == RCC_CFGR_SW_PLL) {
            SystemCoreClockUpdate();    /* 切成功了，把"心跳真值"更新掉 */
            return;
        }
    }
    for (;;) { }    /* 切不过去同样别硬撑 */
}

void SystemCoreClockUpdate(void)
{
    SystemCoreClock = HSE_HZ / PLLM * PLLN / PLLP;
}
