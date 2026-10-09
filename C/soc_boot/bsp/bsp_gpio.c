/* bsp_gpio.c —— 让一个引脚听我们的话
 *
 * 点灯的完整逻辑只有三句：
 *   1. 把引脚方向设成"输出"（MODER）
 *   2. 输出高电平（ODR 对应位置 1）—— 电流流出芯片，LED 亮
 *   3. 输出低电平 —— LED 灭
 *
 * 所谓"点亮一个 SoC"，做到这里就算通了：时钟对了、代码在跑、引脚受控了。
 * 剩下的外设（SPI / I2C / ADC）都是同一套玩法。
 */
#include "bsp_gpio.h"
#include "me1000.h"

void gpio_init_output(uint32_t port, uint32_t pin)
{
    /* 一个引脚占 MODER 里的 2 个 bit：00 输入、01 输出、10 复用、11 模拟 */
    uint32_t pos = pin * 2u;
    REG_MODIFY(port + GPIO_MODER, 3u << pos, (uint32_t)GPIO_MODE_OUTPUT << pos);
}

void gpio_set(uint32_t port, uint32_t pin)
{
    REG_SET_BITS(port + GPIO_ODR, 1u << pin);
}

void gpio_clear(uint32_t port, uint32_t pin)
{
    REG_CLR_BITS(port + GPIO_ODR, 1u << pin);
}

void gpio_toggle(uint32_t port, uint32_t pin)
{
    uint32_t mask = 1u << pin;
    REG_MODIFY(port + GPIO_ODR, mask, (REG_R(port + GPIO_ODR) & mask) ? 0u : mask);
}

uint32_t gpio_read_output(uint32_t port, uint32_t pin)
{
    /* 回读的意义：写寄存器不等于硬件一定照做（时钟没开、引脚被复用占走…）。
     * 回读一次，等于让芯片自己签字确认。 */
    return (REG_R(port + GPIO_ODR) >> pin) & 1u;
}

/* 补充：真实芯片的 ODR 更适合用 BSRR 寄存器去置位/清位，
 * 因为 "读 ODR -> 改一位 -> 写回" 这个动作如果被中断打断，会丢状态。
 * 这里为了把主线讲清楚，先用最朴素的读改写。 */
