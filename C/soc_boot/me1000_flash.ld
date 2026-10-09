/* ==================================================================
 * link.ld —— 链接脚本：决定"每一段东西最终住在芯片的哪里"
 *
 * 编译只管把 C 变成机器码，链接才决定这些机器码落在哪个地址。
 * 芯片上内存是硬约束（Flash 64K / SRAM 20K，就这么点），
 * 所以地址不能像 PC 那样由系统随便给 —— 必须我们自己写死。
 *
 * 三句话概括这个文件：
 *   代码和常量放 Flash，变量放 SRAM；
 *   .data 的"出厂位置"在 Flash，"运行位置"在 SRAM，开机自己搬；
 *   .bss 只占 SRAM 不算尺寸，开机自己清零。
 * ================================================================== */

ENTRY(Reset_Handler)          /* 整个程序的入口，不是 main */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

/* 栈顶取 SRAM 末尾：栈向下增长，从最高地址往下用，最不容易撞到别人 */
_estack = ORIGIN(SRAM) + LENGTH(SRAM);

SECTIONS
{
    /* 向量表必须排在最前面：硬件的"复位行为"就是去地址 0 取前两个字，
     * 换个顺序芯片就找不到启动入口。KEEP 是防止 --gc-sections 把它回收掉。 */
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    /* 代码 + 只读常量。两者都是"只想读、掉电不丢"，放一起最省 Flash。 */
    .text :
    {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);        /* 末尾对齐，否则后面空的段会被撑出几个字节 */
        _etext = .;
    } > FLASH

    /* ---- C / C++ 的"构造清单" ----
     * 全局对象的构造函数、以及标了 __attribute__((constructor)) 的函数，
     * 编译器不会直接调用它们，而是把地址记在这张表里；开机后由 Reset_Handler
     * 调用的 __libc_init_array() 从 _start 走到 _end 逐个执行。
     * 少了这两段，__libc_init_array 会报符号找不到（或者你的构造函数永不执行）。
     * 纯 C 且没有构造函数时，_start 和 _end 相等，不占字节。 */
    .preinit_array : ALIGN(4)
    {
        __preinit_array_start = .;
        KEEP(*(.preinit_array))
        __preinit_array_end = .;
    } > FLASH

    .init_array : ALIGN(4)
    {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    } > FLASH

    .fini_array : ALIGN(4)
    {
        __fini_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        __fini_array_end = .;
    } > FLASH

    /* .data 段：有初值的全局变量
     *   > SRAM     运行时住在 SRAM（可读可写）
     *   AT> FLASH  但"出厂映像"存在 Flash（掉电不丢）
     *   loader 就是 Reset_Handler 里那段拷贝循环。 */
    _sidata = LOADADDR(.data);
    .data :
    {
        _sdata = .;              /* . 是"当前位置"，链接脚本里最常用的符号 */
        *(.data*)
        _edata = .;
    } > SRAM AT> FLASH

    /* .bss 段：没初值的全局变量。
     * 只声明"我要这么多空间"，不占 Flash（全 0 的东西不值得存），
     * 开机由 Reset_Handler 清零。用 NOLOAD 明确告诉链接器别写进映像。 */
    .bss (NOLOAD) :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > SRAM

    /* 编译器/调试用的信息，不占 Flash，保留着方便 gdb 看源码 */
    .comment : { *(.comment) }
    .ARM.attributes : { *(.ARM.attributes) }

    /* 内存用超了要立刻报错，而不是生成一个烧进去就跑飞的固件 */
    ASSERT(. <= ORIGIN(SRAM) + LENGTH(SRAM), "RAM 段超出 SRAM 容量!")
}
