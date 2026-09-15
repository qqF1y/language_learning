#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 9：转义字符与 ANSI 颜色

对应 build_rootfs 中的：
    input('\\033[32m[EDGE INFO] Rootfs already exists ...[Y/n] \\033[0m')
    line = 'FIRMWARE_VER:1.0\\n'

要点：
- \\n 表示换行。
- \\033[32m 是绿色文字开始，\\033[31m 红色，\\033[33m 黄色。
- \\033[0m 恢复默认颜色（必须复位，否则后续输出也变色）。
"""

# \n 换行
print('第一行\n第二行')

# ANSI 颜色
print('\033[32m[EDGE INFO] 构建 rootfs 成功！\033[0m')
print('\033[31m[EDGE ERROR] 挂载 rootfs 失败\033[0m')
print('\033[33m[EDGE WARN] 注意磁盘空间不足\033[0m')
