/* host_entry.c —— PC 版专用的入口（真机上没有这个文件，整个 sim/ 都不进固件）
 *
 * 真机上，"从 0 到 main" 是硬件 + startup/startup_me1000.c 干的；
 * PC 上没有向量表、没有 Reset_Handler，所以就由这个函数把同样的步骤走一遍，
 * 然后再调用固件里那个一模一样的 SystemInit() 和 main()。
 *
 * 换句话说：它是"主机版的启动代码"，和 startup_me1000.c 是同一个角色的两种实现。
 */
#include <stdio.h>

#include "me1000.h"
#include "system_me1000.h"
#include "soc_sim.h"

/* 固件里那个 main。编译时用 -Dmain=firmware_main 换了个名字而已，
 * src/main.c 一个字符都没改 —— 这就是"同一份源码在 PC 上跑"的含义。 */
int firmware_main(void);

int main(void)
{
    printf("=============== 主机模拟：真实芯片上电后要走的路 ===============\n");
    printf("  [硬件]   复位：CPU 去地址 0 取向量表（代码放在 Flash，起始地址 0x%08x）\n",
           (unsigned)FLASH_BASE);
    printf("  [硬件]   向量表[0] 装进 SP = _estack = 0x%08x   <- me1000_flash.ld 算出来的\n",
           (unsigned)SRAM_END);
    printf("  [硬件]   向量表[1] 装进 PC = Reset_Handler       <- startup/startup_me1000.c\n");
    printf("  [startup] 把 .data 从 Flash 拷到 SRAM            <- 同一文件里的拷贝循环\n");
    printf("  [startup] 把 .bss 清零                           <- 保证全局变量初值为 0\n");
    printf("  [startup] 调 SystemInit() -> __libc_init_array() -> main()\n");
    printf("  [主机]   前面几步 PC 上由 ELF 装载器和 C 运行时代劳；\n");
    printf("           但 SystemInit 和 main 属于固件，这里照调不误。\n");
    printf("           最后做这一步，是为了让它们要碰的寄存器在 PC 上也真实存在：\n");

    soc_sim_init();

    printf("--------- 从这里开始，输出的每一行都来自同一份 system / bsp / src/main.c ---------\n");

    /* __libc_init_array() 在主机上不用我们管（glibc 里没有这个符号，
     * .init_array 由 ELF 装载器自己处理），所以这里只补上 SystemInit。 */
    SystemInit();
    return firmware_main();
}
