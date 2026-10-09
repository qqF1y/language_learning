/* system_me1000.h —— 芯片级的"底层初始化"接口（行业里对应 system_stm32f4xx.h）
 *
 * 这个命名不是随便起的：各家 SDK 里 system_xxx 这一层管的是
 * "和芯片本身有关、和具体应用无关"的事 —— 时钟树、Flash 等待周期、向量表偏移……
 * 它在 main 之前被启动文件调用，所以 main 里不需要再配时钟。
 */
#ifndef SYSTEM_ME1000_H
#define SYSTEM_ME1000_H

#include <stdint.h>

/* 当前 CPU 心跳（SYSCLK）。所有外设算分频都拿它当基准：
 * 串口波特率、定时器周期、延时……全工程只需要这一个"真值来源"。 */
extern uint32_t SystemCoreClock;

/* 复位后由启动文件调用，跑在 main 之前：配时钟树 */
void SystemInit(void);

/* 时钟被别处改过之后，用它把 SystemCoreClock 重新算一遍 */
void SystemCoreClockUpdate(void);

#endif /* SYSTEM_ME1000_H */
