#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 4：if / elif / else 条件分支

对应 build_rootfs 中的：
    if arch == 'arm64':
        arch_alias = 'aarch64'
    else:
        arch_alias = 'arm'

要点：
- if 后面跟条件，条件成立执行对应代码块。
- elif 是「否则如果」，可以写多个。
- else 是「以上都不成立」。
- 代码块靠缩进区分，缩进必须一致。
"""


def get_arch_alias(arch):
    if arch == 'arm64':
        return 'aarch64'
    elif arch == 'arm':
        return 'armhf'
    else:
        return arch


print(get_arch_alias('arm64'))   # aarch64
print(get_arch_alias('arm'))     # armhf
print(get_arch_alias('x86_64'))  # x86_64
