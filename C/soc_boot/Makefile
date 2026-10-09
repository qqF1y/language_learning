# =====================================================================
#  soc_boot —— 从 0 到 main，点亮一块 SoC
#
#    make         两个版本都编
#    make run     编好并运行"主机模拟版"（推荐先看这个，能看见整个过程）
#    make elf     交叉编译出真机固件 build/soc_boot.elf / .bin / .map
#    make size    看看固件占了多少 Flash / RAM
#    make clean
#
#  同一份固件源码，两套编译方式：
#
#    主机版 ：gcc + -DSOC_SIM
#             寄存器读写被换成一个函数，转交给 sim/soc_sim.c 里的虚拟硬件，
#             于是"真机代码"能在 PC 上跑起来、输出能看见。
#
#    真机版 ：arm-none-eabi-gcc + me1000_flash.ld + startup/startup_me1000.c
#             生成真正能烧进芯片的二进制。往 .bin 里看一眼，
#             头 8 个字节就是栈顶地址和 Reset_Handler 的地址。
# =====================================================================

CROSS   ?= arm-none-eabi-
CC       = gcc
CROSSCC  = $(CROSS)gcc
OBJCOPY  = $(CROSS)objcopy
SIZE     = $(CROSS)size

BUILD := build
INC   := -Iinclude -Isim
HDRS  := $(wildcard include/*.h) $(wildcard sim/*.h)
LDSCRIPT := me1000_flash.ld

# 真机固件 = 启动文件 + 下面这些"两边共用"的代码（一个字符都不差）
TGT_ONLY := startup/startup_me1000.s
SHARED   := system/system_me1000.c bsp/bsp_gpio.c bsp/bsp_uart.c \
            src/main.c src/me1000_it.c
# 只在主机上编（虚拟硬件 + PC 版启动代码）
HOST_ONLY := sim/soc_sim.c sim/host_entry.c

WARN       := -Wall -Wextra
HOST_FLAGS := -std=gnu11 $(WARN) -O0 -g -DSOC_SIM $(INC)
TGT_FLAGS  := -std=gnu11 $(WARN) -O2 -ffreestanding -mcpu=cortex-m4 -mthumb \
              $(INC) -nostartfiles -T$(LDSCRIPT) -Wl,-Map=$(BUILD)/soc_boot.map

HOST_BIN := $(BUILD)/soc_boot_host
ELF      := $(BUILD)/soc_boot.elf
BIN      := $(BUILD)/soc_boot.bin

# 主机版把目录结构也保留下来（build/bsp/bsp_gpio.o 对 bsp/bsp_gpio.c），
# 好处是编译日志和源码路径一一对应，出错时不用猜是哪个文件。
HOST_SRCS := $(SHARED) $(HOST_ONLY)
HOST_OBJ  := $(patsubst %.c,$(BUILD)/%.o,$(HOST_SRCS))

all: host elf

# ------------------------------ 主机模拟版 ------------------------------
host: $(HOST_BIN)

$(BUILD)/%.o: %.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_FLAGS) -c $< -o $@

# 固件那个 main 在主机上改名成 firmware_main（src/main.c 本身不动），
# 好让 sim/host_entry.c 自己当 main：先演一遍启动流程，再调用它。
$(BUILD)/src/main.o: src/main.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_FLAGS) -Dmain=firmware_main -c $< -o $@

$(HOST_BIN): $(HOST_OBJ)
	$(CC) $(HOST_FLAGS) $(HOST_OBJ) -o $@

run: $(HOST_BIN)
	@./$(HOST_BIN)

# ------------------------------ 真机固件版 ------------------------------
elf: $(BIN)

$(ELF): $(TGT_ONLY) $(SHARED) $(LDSCRIPT) $(HDRS)
	@mkdir -p $(BUILD)
	$(CROSSCC) $(TGT_FLAGS) $(TGT_ONLY) $(SHARED) -o $@
	@echo
	@$(SIZE) $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@
	@ls -l $@ | awk '{print "  -> " $$NF "  " $$5 " 字节，这就是要烧进 Flash 的映像"}'

size: $(ELF)
	$(SIZE) $(ELF)

clean:
	rm -rf $(BUILD)

.PHONY: all host run elf size clean
