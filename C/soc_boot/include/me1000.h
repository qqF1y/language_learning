/* me1000.h —— 器件头文件（行业里对应 stm32f4xx.h 这种）
 *
 * 软件到底是怎么"碰到"硬件的？
 * 一句话：片上外设 = 一段有地址的内存。往那个地址写数，芯片就会动；
 *         读那个地址，就是问芯片"你现在怎么样了"。
 *
 * 这个文件把所有裸地址、寄存器位定义、板子接线集中在一起。
 * 别的 .c 里不应该再出现任何魔法数字 —— 这是嵌入式代码可读性的第一条纪律。
 * 换一颗芯片，通常只需要换掉这个文件，外加链接脚本和 Makefile 里的 -mcpu。
 */
#ifndef ME1000_H
#define ME1000_H

#include <stdint.h>
#include <stddef.h>

/* ==================================================================
 * 1. 地址映射（照着芯片手册抄，抄错就点不亮）
 * ================================================================== */
#define FLASH_BASE    0x08000000u                  /* 掉电不丢，代码住这 */
#define SRAM_BASE     0x20000000u                  /* 掉电就丢，变量住这 */
#define SRAM_SIZE     (20u * 1024u)
#define SRAM_END      (SRAM_BASE + SRAM_SIZE)      /* link.ld 里的 _estack 就是它 */

#define PERIPH_BASE   0x40000000u                  /* 外设区：一整片都是寄存器 */
#define PERIPH_SIZE   (256u * 1024u)

#define RCC_BASE      (PERIPH_BASE + 0x0000u)      /* 时钟：整个芯片的"心脏起搏器" */
#define GPIOA_BASE    (PERIPH_BASE + 0x0800u)      /* 引脚 */
#define UART0_BASE    (PERIPH_BASE + 0x1000u)      /* 串口：调试的眼睛 */

/* ---- RCC：CR 管"开没开"，CFGR 管"最终用哪一个" ---- */
#define RCC_CR              0x00u
#define RCC_CR_HSEON        (1u << 16)   /* 写 1：请求打开外部晶振 */
#define RCC_CR_HSERDY       (1u << 17)   /* 读 1：晶振真的起振了（硬件回置） */
#define RCC_CR_PLLON        (1u << 24)   /* 写 1：请求打开 PLL */
#define RCC_CR_PLLRDY       (1u << 25)   /* 读 1：PLL 真的锁定了 */

#define RCC_PLLCFGR         0x04u        /* 倍频器的三个参数都住在这个寄存器里 */
#define RCC_PLLCFGR_PLLM_POS 0u          /* 输入分频：HSE / PLLM */
#define RCC_PLLCFGR_PLLN_POS 6u          /* 倍频系数：x PLLN */
#define RCC_PLLCFGR_PLLP_POS 16u         /* 输出分频：/ PLLP（编码值 0 表示 2） */

#define RCC_CFGR            0x08u
#define RCC_CFGR_SW_POS     0u
#define RCC_CFGR_SW_MASK    (3u << RCC_CFGR_SW_POS)   /* 写：我要切到谁 */
#define RCC_CFGR_SW_PLL     2u
#define RCC_CFGR_SWS_POS    2u
#define RCC_CFGR_SWS_MASK   (3u << RCC_CFGR_SWS_POS)  /* 读：现在实际是谁 */

/* ---- GPIO：MODER 管方向，ODR 管高低电平 ---- */
#define GPIO_MODER          0x00u
#define GPIO_MODE_INPUT     0u
#define GPIO_MODE_OUTPUT    1u
#define GPIO_ODR            0x14u

/* ---- UART：SR 看状态，DR 收发数据，BRR 定波特率，CR1 是总开关 ---- */
#define USART_SR            0x00u
#define USART_SR_TXE        (1u << 7)    /* 发送寄存器空了，可以塞下一个字节 */
#define USART_SR_TC         (1u << 6)    /* 整个字节发完了 */
#define USART_DR            0x04u
#define USART_BRR           0x08u
#define USART_CR1           0x0Cu
#define USART_CR1_UE        (1u << 13)   /* UART 总使能 */
#define USART_CR1_TE        (1u << 3)    /* 打开发送 */

/* ==================================================================
 * 2. 器件名和板子接线（只在这里写一次，驱动和模拟器都引用它）
 * ================================================================== */
#define DEVICE_NAME   "ME1000"          /* 这颗 SoC 的型号，会出现在文件名里 */
#define BOARD_NAME    "MAGIC-EVB v1"    /* 板子型号：同一颗芯片能做成很多块板 */
#define LED_PORT      GPIOA_BASE
#define LED_PIN       5u
#define LED_PIN_NAME  "PA5"      /* 原理图：PA5 --[LED]--[限流电阻]-- GND */

/* ==================================================================
 * 3. 访问层：全工程唯一直接碰寄存器的地方
 *
 *   真机：*(volatile uint32_t *)addr = val;   // 一条 STR 指令，直达硬件
 *   主机：转发给 sim/soc_sim.c 里的"虚拟硬件"，让它做出反应
 *
 *   上面四行驱动代码两边一模一样 —— 这就是"可移植"的全部秘密：
 *   把不确定的东西（硬件）压缩到最薄的一层里。
 * ================================================================== */
#ifdef SOC_SIM
void     sim_mmio_write(uintptr_t addr, uint32_t val);
uint32_t sim_mmio_read(uintptr_t addr);

#define REG_W(addr, val) sim_mmio_write((uintptr_t)(addr), (uint32_t)(val))
#define REG_R(addr)      sim_mmio_read((uintptr_t)(addr))
#else
#define REG_W(addr, val) (*(volatile uint32_t *)(uintptr_t)(addr) = (uint32_t)(val))
#define REG_R(addr)      (*(volatile uint32_t *)(uintptr_t)(addr))
#endif

/* volatile 一个都不能省：不加，编译器会认为"写进去又没人读"，
 * 直接把整条语句优化掉 —— 这在嵌入式里是最经典的"代码消失了"事故。 */
#define REG_SET_BITS(addr, mask) \
    REG_W((addr), REG_R(addr) | (uint32_t)(mask))
#define REG_CLR_BITS(addr, mask) \
    REG_W((addr), REG_R(addr) & ~(uint32_t)(mask))
#define REG_MODIFY(addr, mask, val) \
    REG_W((addr), (REG_R(addr) & ~(uint32_t)(mask)) | ((uint32_t)(val) & (uint32_t)(mask)))

#endif /* ME1000_H */
