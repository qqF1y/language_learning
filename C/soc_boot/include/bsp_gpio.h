/* bsp_gpio.h —— 板级驱动：GPIO（行业里对应 bsp_xxx / hal_gpio 这类命名）
 *
 * BSP = Board Support Package，板级支持包：把"这块板子上有什么"包装成
 * 应用代码能直接用的函数。main.c 只应该看到 gpio_set(端口, 引脚)，
 * 不需要知道寄存器叫什么、位怎么摆。
 *
 * 点灯的本质就一句话：让一个引脚输出高电平，电流从芯片流出去点亮 LED。
 */
#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdint.h>

void     gpio_init_output(uint32_t port, uint32_t pin);
void     gpio_set(uint32_t port, uint32_t pin);        /* 输出高电平 */
void     gpio_clear(uint32_t port, uint32_t pin);      /* 输出低电平 */
void     gpio_toggle(uint32_t port, uint32_t pin);
uint32_t gpio_read_output(uint32_t port, uint32_t pin);/* 回读，验证写进去没有 */

#endif /* BSP_GPIO_H */
