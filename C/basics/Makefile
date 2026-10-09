# C 语言经典例程 Makefile
#
#   make                 编译全部例程到 build/
#   make run             编译并依次运行全部例程
#   make one FILE=05_pointers_basics
#                        只编译并运行某一个例程
#   make SAN=1 run       带 AddressSanitizer/UBSan 运行(用来抓内存错误)
#   make clean           清理

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -g -O0
BUILD   := build

# 单文件例程(形如 01_xxx.c ... 17_xxx.c)
SINGLE  := $(wildcard [0-9][0-9]_*.c)
BINS    := $(patsubst %.c,$(BUILD)/%,$(SINGLE))

# 多文件编译例程
MULTI_DIR   := 18_multi_file
MULTI_BIN   := $(BUILD)/18_multi_file
MULTI_SRCS  := $(MULTI_DIR)/main.c $(MULTI_DIR)/stats.c

ifdef SAN
CFLAGS += -fsanitize=address,undefined
endif

all: $(BINS) $(MULTI_BIN)

$(BUILD)/%: %.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

$(MULTI_BIN): $(MULTI_SRCS) $(MULTI_DIR)/stats.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(MULTI_SRCS) -o $@

run: all
	@for b in $(BINS) $(MULTI_BIN); do \
		echo "==================== $$b ===================="; \
		./$$b < /dev/null || echo "(退出码 $$?)"; \
		echo; \
	done

one: $(BUILD)/$(FILE)
	@./$(BUILD)/$(FILE)

clean:
	rm -rf $(BUILD)

.PHONY: all run one clean
