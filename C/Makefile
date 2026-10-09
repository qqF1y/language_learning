# C 目录总入口：基础例程 + 启动 demo + 专题 demo
#
#   make             全部编译
#   make run         跑 soc_boot 启动 demo（推荐先看这个）
#   make run-basics  依次跑 18 个基础例程
#   make run-pinctrl 跑 pinctrl 原理 demo（纯用户态）
#   make clean

all: basics soc pinctrl

basics:
	$(MAKE) -C basics

soc:
	$(MAKE) -C soc_boot

pinctrl:
	$(MAKE) -C pinctrl_demo

run:
	$(MAKE) -C soc_boot run

run-basics:
	$(MAKE) -C basics run

run-pinctrl:
	$(MAKE) -C pinctrl_demo run

# 内核驱动章节（字符设备 / tty / pinctrl）目录存在时才编，免得 make 直接失败
driver:
	@if [ -d kernel_driver ]; then $(MAKE) -C kernel_driver; \
	else echo "跳过 kernel_driver/（目录不在工作区）"; fi

run-driver:
	@if [ -d kernel_driver ]; then $(MAKE) -C kernel_driver sim; \
	else echo "跳过 kernel_driver/（目录不在工作区）"; fi

clean:
	$(MAKE) -C basics clean
	$(MAKE) -C soc_boot clean
	$(MAKE) -C pinctrl_demo clean
	@if [ -d kernel_driver ]; then $(MAKE) -C kernel_driver clean; fi

.PHONY: all basics soc pinctrl driver run run-basics run-pinctrl run-driver clean
