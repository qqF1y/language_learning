/* bsp_uart.c —— 裸机上的 printf
 *
 * 为什么不能直接 #include <stdio.h> 用 printf？
 * 因为 PC 上的 printf 最终要落到"文件描述符 / 系统调用"上，
 * 而芯片上既没有操作系统也没有文件系统，那些函数根本无处可去。
 *
 * 所以裸机的输出分两层：
 *   上层：格式化（怎么把数字变成字符）—— 和 PC 上完全一样，纯软件
 *   下层：uart_putc（怎么把字符送出去）—— 换成别的屏/网络也一样
 *
 * 这个分层就是所有日志库的雏形：换后端，不动业务代码。
 */
#include "bsp_uart.h"
#include "system_me1000.h"
#include "me1000.h"

#include <stdarg.h>

void uart_init(uint32_t baud)
{
    /* 真机上这里还要先把 TX/RX 引脚配成"复用功能"并选对复用号，
     * 本例跳过，重点是"串口也是一堆寄存器"。 */
    /* 分频值 = 总线时钟 / 波特率。总线时钟从 SystemCoreClock 推出来，
     * 这就是为什么时钟必须在串口之前配好 —— 心跳不准，出去的波形就是乱的。 */
    uint32_t pclk = SystemCoreClock / 4u;         /* UART 挂在 APB 总线上 */
    REG_W(UART0_BASE + USART_BRR, pclk / baud);

    REG_SET_BITS(UART0_BASE + USART_CR1, USART_CR1_UE | USART_CR1_TE);
}

void uart_putc(char c)
{
    /* 别写太快：硬件搬走一个字节需要时间，必须等 TXE（发送寄存器空）置位。
     * 忘了等，就会丢字符 —— 这是新手最常见的"打印缺字"原因。 */
    while ((REG_R(UART0_BASE + USART_SR) & USART_SR_TXE) == 0u) {
    }
    REG_W(UART0_BASE + USART_DR, (uint32_t)(uint8_t)c);
}

void uart_puts(const char *s)
{
    if (s == NULL) {
        return;
    }
    while (*s != '\0') {
        uart_putc(*s++);
    }
}

void uart_putu32(uint32_t v)
{
    /* 数字 -> 字符只能一位一位来：先取最低位，所以得到的是倒序，最后倒着打 */
    char     buf[12];
    uint32_t n = 0;

    if (v == 0u) {
        uart_putc('0');
        return;
    }
    while (v > 0u) {
        buf[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (n > 0u) {
        uart_putc(buf[--n]);
    }
}

void uart_puthex32(uint32_t v)
{
    uart_puts("0x");
    for (int32_t i = 7; i >= 0; i--) {
        uint32_t nib = (v >> (i * 4)) & 0xFu;
        uart_putc((char)(nib < 10u ? ('0' + nib) : ('a' + (nib - 10u))));
    }
}

void uart_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            uart_putc(*p);
            continue;
        }
        p++;
        if (*p == '\0') {
            uart_putc('%');
            break;
        }
        switch (*p) {
        case 's': uart_puts(va_arg(ap, const char *)); break;
        case 'u': uart_putu32(va_arg(ap, uint32_t));   break;
        case 'x': uart_puthex32(va_arg(ap, uint32_t)); break;
        case 'c': uart_putc((char)va_arg(ap, int));    break;
        case '%': uart_putc('%');                      break;
        default:  uart_putc('%'); uart_putc(*p);       break;
        }
    }

    va_end(ap);
}
