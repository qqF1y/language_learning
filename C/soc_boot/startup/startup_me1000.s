/* startup_me1000.s —— 启动文件，从"0"到 main 的那段路
 *
 * 这就是各家 SDK 里那个 startup_<器件>.s，纯汇编，不依赖任何编译器扩展。
 * 内容三块，顺序和 ST 的官方启动文件一样：
 *
 *   1. 向量表          g_pfnVectors：放 Flash 最前面，前两格是 SP 和 PC
 *   2. Reset_Handler   搬 .data -> 清 .bss -> SystemInit -> __libc_init_array -> main
 *   3. 中断处理函数     全部 .weak + .thumb_set 到 Default_Handler
 *
 * 为什么这一段只能用汇编？因为它运行的时候 C 运行环境还不存在：
 *   - .data 段还在 Flash 里没搬进来，读全局变量会读到错的东西
 *   - .bss 段还没清零，读到的是随机值
 *   - 只有栈指针是硬件替我们设好的，这已经是唯一的"既得福利"了
 *
 * 汇编语法要点：以 . 开头的是汇编器指令（不生成指令，只指挥汇编器），
 * 其余是要真生成的指令。前缀 L 的是局部符号，用 =符号 让汇编器自己建字面量池。
 */

  .syntax unified
  .cpu cortex-m4
  .fpu softvfp
  .thumb

  .global g_pfnVectors
  .global Reset_Handler

/* ==================================================================
 * 一、向量表 = 芯片的开机流程单，必须摆在 Flash 最前面
 *
 * 硬件复位只做两件事：取第一个字当 SP、第二个字当 PC。
 * 所以 [0] 是"数据"（栈顶地址），不是函数指针；
 * [1] 之后才是真正的入口，下标 = 异常号 + 16。
 *
 * 别和 8051 / ARM7TDMI 那套混淆：那些是老式做法，PC 真的从 0 开始跑，
 * 地址 0 上放的是一条跳转指令。Cortex-M 改成"查表"了 ——
 * 硬件把 0x00000000 的内容装进 SP、0x00000004 的内容装进 PC，
 * 所以复位后的 PC = 表里第二格的值（本例 0x08000049），而不是 0。
 * 硬件是来"读"这张表的，从来没执行过地址 0 上的东西。
 *
 * 两个随之而来的约束：
 *   1. 本表链接在 0x08000000，而硬件读的是 0x00000000 —— 靠芯片的
 *      启动别名（boot 引脚把 Flash 映射到 0 地址）让同一份字节两边都能看到。
 *   2. PC 取值的最低位必须是 1（Thumb 位），否则一复位就 InvState 故障。
 *      这个是链接器算函数地址时自动带上的。
 *
 * 为什么它能在 C 运行环境还不存在的时候就生效？
 * 因为它是"数据"不是"代码"：硬件只是来读它，从来不执行它。
 * 而每个元素都是链接期就能定下来的地址，由链接器直接填进映像里，
 * 不需要任何运行期初始化。
 * ================================================================== */
  .section .isr_vector, "a", %progbits
  .balign 4                     /* 表里全是 32 位字，段必须 4 字节对齐 */
  .type g_pfnVectors, %object

g_pfnVectors:
  .word _estack                 /* [0]  栈顶地址（0x20005000），硬件当数用 */
  .word Reset_Handler           /* [1]  复位：芯片的第一条指令（末位 1 = Thumb 位） */
  .word NMI_Handler             /* [2]   -14 */
  .word HardFault_Handler       /* [3]   -13  程序跑飞了最后都掉这里 */
  .word MemManage_Handler       /* [4]   -12 */
  .word BusFault_Handler        /* [5]   -11 */
  .word UsageFault_Handler      /* [6]   -10 */
  .word 0                       /* [7]  架构保留，硬件不会来取 */
  .word 0                       /* [8] */
  .word 0                       /* [9] */
  .word 0                       /* [10] */
  .word SVC_Handler             /* [11]  -5  系统调用（跑 RTOS 时用） */
  .word DebugMon_Handler        /* [12]  -4 */
  .word 0                       /* [13] 保留 */
  .word PendSV_Handler          /* [14]  -2  RTOS 做任务切换用 */
  .word SysTick_Handler         /* [15]  -1  系统滴答，裸机的时间基准 */
  .word IRQ0_Handler            /* [16] IRQ0 起，下面是厂商的外设中断 */
  .word IRQ1_Handler            /* [17] */

  .size g_pfnVectors, .-g_pfnVectors

/* ==================================================================
 * 二、复位入口：整个工程的第一条指令
 *
 * 常规启动文件的四步，顺序不能换：
 *   设栈 -> 搬 .data -> 清 .bss -> 调 SystemInit / __libc_init_array / main
 * ================================================================== */
  .section .text.Reset_Handler, "ax", %progbits
  .type Reset_Handler, %function

Reset_Handler:
  /* ---- 第 0 步：设栈指针 ----
   * 硬件已经按向量表[0]装过一次了，这里再设一次是常规做法：
   * 被 bootloader 或调试器直接跳进来时，硬件那一步可能没发生过。 */
  ldr   sp, =_estack

  /* ---- 第 1 步：把 .data 从 Flash 搬到 SRAM ----
   * 有初值的全局变量，出厂映像存在 Flash（掉电不丢），
   * 运行时要能读写，必须在 SRAM 里，所以开机搬一次。 */
  ldr   r0, =_sdata        /* 目标：SRAM 里的运行位置 */
  ldr   r1, =_edata        /* 目标结尾 */
  ldr   r2, =_sidata       /* 源：Flash 里的出厂数据 */
  movs  r3, #0             /* 偏移量 */
  b     .Lcopy_check
.Lcopy:
  ldr   r4, [r2, r3]
  str   r4, [r0, r3]
  adds  r3, r3, #4
.Lcopy_check:
  adds  r4, r0, r3
  cmp   r4, r1
  bcc   .Lcopy             /* bcc = 无符号小于则跳（这里判断"还没搬完"） */

  /* ---- 第 2 步：把 .bss 清零 ----
   * 没写初值的全局变量按 C 标准必须是 0。
   * 它们在 Flash 里不占空间（存一堆 0 没意义），只能开机现清零。 */
  ldr   r2, =_sbss
  ldr   r4, =_ebss
  movs  r3, #0
  b     .Lzero_check
.Lzero:
  str   r3, [r2]
  adds  r2, r2, #4
.Lzero_check:
  cmp   r2, r4
  bcc   .Lzero

  /* ---- 第 3 步：按固定顺序进 C ----
   * 走到这里 C 语言才算"能用了"：栈有了、变量有了、BSS 干净了。 */
  bl    SystemInit          /* system/system_me1000.c：时钟树，在 main 之前配好 */
  bl    __libc_init_array   /* C++ 全局对象构造；纯 C 且无构造函数时什么都不做 */
  bl    main                /* src/main.c，用户代码 */

  /* ---- 第 4 步：main 不该返回 ----
   * ST 的启动文件这里写的是 bx lr（返回就进 HardFault）。
   * 这里改成原地停住：跑飞了比停住难查得多。 */
  b     .

  .ltorg                    /* 把上面那些 =符号 生成的常量放在这里 */
  .size Reset_Handler, .-Reset_Handler

/* ==================================================================
 * 三、兜底实现
 * ================================================================== */

/* 所有没被覆盖的中断都掉进来。
 * 真机上死循环很难定位，工程里通常在这里把当前的 PC / LR 存到一个固定地址，
 * 用调试器连上去看现场，比"板子没反应"有用得多。 */
  .section .text.Default_Handler, "ax", %progbits
  .global Default_Handler
  .type Default_Handler, %function

Default_Handler:
  b     .
  .size Default_Handler, .-Default_Handler

/* newlib 的 __libc_init_array() 里会先调一次 _init()。
 * 正常这个符号由 crti.o 提供（C 运行时的一部分），而工程里用 -nostartfiles
 * 不要那套启动代码，所以自己补一个空的 —— 全局构造函数之前的钩子。 */
  .section .text._init, "ax", %progbits
  .weak _init
  .type _init, %function

_init:
  bx    lr
  .size _init, .-_init

/* ==================================================================
 * 四、中断处理函数：全部 weak，默认指向 Default_Handler
 *
 * 这两行是启动文件里的固定套路，效果是：
 *   .weak        这个符号是弱符号 —— 别处若有强符号，就用别处那个
 *   .thumb_set   把它的地址设成 Default_Handler，相当于起个别名
 *
 * 所以 src/me1000_it.c 里定义一个同名函数，就自动顶替掉它，这个文件不用改。
 * ================================================================== */
  .weak      NMI_Handler
  .thumb_set NMI_Handler,Default_Handler

  .weak      HardFault_Handler
  .thumb_set HardFault_Handler,Default_Handler

  .weak      MemManage_Handler
  .thumb_set MemManage_Handler,Default_Handler

  .weak      BusFault_Handler
  .thumb_set BusFault_Handler,Default_Handler

  .weak      UsageFault_Handler
  .thumb_set UsageFault_Handler,Default_Handler

  .weak      SVC_Handler
  .thumb_set SVC_Handler,Default_Handler

  .weak      DebugMon_Handler
  .thumb_set DebugMon_Handler,Default_Handler

  .weak      PendSV_Handler
  .thumb_set PendSV_Handler,Default_Handler

  .weak      SysTick_Handler
  .thumb_set SysTick_Handler,Default_Handler

/* 外设中断：编号从 IRQ0 开始，名字由芯片手册定，数量几个就写几行 */
  .weak      IRQ0_Handler
  .thumb_set IRQ0_Handler,Default_Handler

  .weak      IRQ1_Handler
  .thumb_set IRQ1_Handler,Default_Handler
