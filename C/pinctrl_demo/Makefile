# =====================================================================
#  pinctrl_demo —— pin controller 原理(纯用户态, 不需要内核/root)
#
#    make / make run   跑原理演示(pinctrl_demo.c, 配置写死在 C 里)
#    make dts          跑设备树版(dts_demo.c + board.dts / board-bad.dts)
#    make steps        只跑"配置一个模式"的 4 步, 每步打印寄存器(steps_demo.c)
#    make func         每个函数单独调用一遍, 看输入->输出(func_demo.c)
#    make clean
#
#  想上真机? 真 pinctrl 驱动是 platform driver, 在 probe() 里调
#  pinctrl_register() 注册; 见 README 最后一节, 这里不塞进 demo, 保持"小"。
# =====================================================================

CC      ?= gcc
BUILD   := build
CFLAGS  := -std=gnu11 -Wall -Wextra -O1

BIN       := $(BUILD)/pinctrl_demo
DTS_BIN   := $(BUILD)/dts_demo
STEPS_BIN := $(BUILD)/steps_demo
FUNC_BIN  := $(BUILD)/func_demo

all: steps func run dts

run: $(BIN)
	@./$(BIN)

# 先跑正常板子, 再跑那张写错的 —— 对照着看效果
# (dts_demo 从当前目录读 .dts, 所以要在 pinctrl_demo/ 下跑)
dts: $(DTS_BIN)
	@./$(DTS_BIN) board.dts
	@echo
	@./$(DTS_BIN) board-bad.dts || true   # 这张是故意写错的, 退出码不看

$(BIN): pinctrl_demo.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

$(DTS_BIN): dts_demo.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

# 最基础的一版: 只有"查表 + 写字段", 输出里能看清每次动了哪几位
steps: $(STEPS_BIN)
	@./$(STEPS_BIN)

$(STEPS_BIN): steps_demo.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

# 每个函数一个 demo_xxx(), 单独调用看行为
func: $(FUNC_BIN)
	@./$(FUNC_BIN)

$(FUNC_BIN): func_demo.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all run dts steps func clean
