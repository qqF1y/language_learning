/* bsp_uart.h —— 板级驱动：串口，裸机上唯一的 printf
 *
 * 注意：真机上没有操作系统、没有 libc 的 printf，那些东西要靠
 * 文件系统 / 系统调用，芯片上并不存在。所以自己写一个极简 printf，
 * 底层就是"往 UART 数据寄存器塞一个字节"。
 */
#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>

void uart_init(uint32_t baud);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_putu32(uint32_t v);
void uart_puthex32(uint32_t v);
void uart_printf(const char *fmt, ...);   /* 只支持 %s %u %x %c %% */

#endif /* BSP_UART_H */
