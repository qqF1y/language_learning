/* main.c —— 终点，也是起点
 *
 * 在 PC 上写 C 时，main 是"第一个"函数；在芯片上，main 是"最后一个"被调到的函数：
 * 前面有没有人把栈指针设好、有没有人把带初值的变量搬进 RAM，
 * 都得由启动代码兜住 —— 这就是这个 demo 想让你看见的东西。
 *
 * main 里做的事，正好是 bring-up（点亮一块板子）的最小闭环：
 *   时钟对 -> 有输出（串口）-> 能控制引脚（点灯）
 */
#include "me1000.h"          /* 器件头文件：寄存器地址、寄存器位、板子接线 */
#include "system_me1000.h"   /* SystemCoreClock */
#include "bsp_gpio.h"
#include "bsp_uart.h"

/* ------------------------------------------------------------------
 * 这两个全局变量是故意放着的，它们是"启动代码干过活"的证据：
 *   g_banner 有初值  -> 住在 .data，Reset_Handler 必须把它从 Flash 搬进 SRAM
 *   g_led_on 没初值  -> 住在 .bss， Reset_Handler 必须把它清零
 * 如果搬 .data 或清 .bss 少做一步，下面打印出来的就是乱码或随机数。
 * ------------------------------------------------------------------ */
char     g_banner[] = "[main] " BOARD_NAME " 起来了";   /* .data */
uint32_t g_led_on;                                     /* .bss  */

#ifdef SOC_SIM
#define BLINK_ROUNDS  6u        /* 主机模拟：闪 6 次就收工，方便看输出 */
#define DELAY_LOOPS   20000u    /* 主机上不需要真延时 */
#else
#define BLINK_ROUNDS  0u        /* 真机：0 表示不退出，板子会一直闪 */
#define DELAY_LOOPS   2000000u  /* 真机上靠空转拖时间，肉眼能看出闪 */
#endif

static void delay(volatile uint32_t n)
{
    while (n > 0u) {
        n--;
    }
}

int main(void)
{
    /* 1. 时钟不用管：启动文件在调 main 之前已经调过 SystemInit() 了。
     *    这是工程上的分工 —— main 里只留"这块板子要干什么"，
     *    "芯片本身怎么初始化"归 system 层，所以这里直接就能用 SystemCoreClock。 */

    /* 2. 把"嘴"接上，从这一刻起才有办法看见程序在干什么 */
    uart_init(115200);

    uart_printf("\n%s，SYSCLK = %u Hz\n", g_banner, SystemCoreClock);
    uart_puts("[main] 能运行到这里，说明这一串全对了：\n");
    uart_puts("       向量表 -> 栈指针 -> .data 搬运 -> .bss 清零 -> SystemInit -> 串口\n");

    /* 3. 点亮 LED：把引脚配成输出 */
    gpio_init_output(LED_PORT, LED_PIN);
    uart_printf("[main] LED 引脚 %s 已配成输出，开始闪：\n", LED_PIN_NAME);

#if BLINK_ROUNDS > 0u
    uint32_t rounds = 0u;
#endif

    /* 4. 嵌入式程序的正常结尾就是一个永不退出的循环：
     *    退出了没人接，芯片会跑飞。 */
    for (;;) {
        delay(DELAY_LOOPS);

        g_led_on = (g_led_on == 0u) ? 1u : 0u;
        if (g_led_on != 0u) {
            gpio_set(LED_PORT, LED_PIN);
        } else {
            gpio_clear(LED_PORT, LED_PIN);
        }

        uart_printf("[led ] %s，回读 %s 的输出位 = %u\n",
                    (g_led_on != 0u) ? "亮" : "灭",
                    LED_PIN_NAME,
                    gpio_read_output(LED_PORT, LED_PIN));

#if BLINK_ROUNDS > 0u
        if (++rounds >= BLINK_ROUNDS) {
            break;
        }
#endif
    }

    uart_puts("[main] 主机模拟到此结束；真机上这个 for 永不退出，板子会一直闪。\n");
    return 0;
}
