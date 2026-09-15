#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 10：字符串 split() 与 int() 类型转换

对应 build_rootfs 中的：
    rootfs_taskrel = '...%s-%s.tar.gz' % (..., rootfs_type.split('-')[0])
    cmd = '... %dM' % (..., int(rootfs_size))

要点：
- 'a-b-c'.split('-') 按 - 切分，返回列表 ['a', 'b', 'c']。
- [0] 取列表第一个元素。
- int('20480') 把字符串转成整数，便于运算和 %d 格式化。
"""

# split 后取第一段
rootfs_type = 'debian-minimal'
osname = rootfs_type.split('-')[0]
print('osname =', osname)

# 字符串数字转整数
rootfs_size = '20480'
size_int = int(rootfs_size)
print('size_int =', size_int, '类型:', type(size_int).__name__)

# 转成 int 后才能用 %d 或做算术
cmd = 'resize.sh rootfs.img %dM' % int(rootfs_size)
print('cmd =', cmd)
