#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 7：sys.exit 退出程序

对应 build_rootfs 中的：
    if os.path.exists(chroot_bin) == False:
        EDGE_ERR('coreutils is not installed')
        sys.exit(1)

要点：
- sys.exit(n) 立即结束进程。
- n=0 表示正常退出，n 非 0 表示异常退出（供上层脚本判断）。
- 使用前需 import sys。
"""
import os
import sys


def check_coreutils():
    chroot_bin = '/usr/sbin/chroot'
    if os.path.exists(chroot_bin) == False:
        print('[ERROR] coreutils is not installed')
        sys.exit(1)  # 非 0 退出码，程序到此为止
    print('coreutils 已安装')


check_coreutils()
print('继续执行后续构建流程...')
