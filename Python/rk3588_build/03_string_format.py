#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 3：字符串格式化（% 占位符）

对应 build_rootfs 中的：
    rootfs_base = '%s/rootfs/images/%s/%s%s-base.tar.gz' % (...)
    cmd = '%s/build/scripts/resize.sh ... %dM' % (..., int(rootfs_size))

要点：
- %s：字符串占位符。
- %d：整数占位符。
- 多个占位符时，右边用元组 (a, b, c) 依次填入。
"""

root_path = '/home/qiurangfei/PRJ/a2614/rk3588_xenomai'
arch = 'arm64'
rootfs_osname = 'debian'
rootfs_version = '12'

# 4 个 %s 依次被替换
rootfs_base = '%s/rootfs/images/%s/%s%s-base.tar.gz' % (
    root_path, arch, rootfs_osname, rootfs_version
)
print('rootfs_base =', rootfs_base)

# %d 用于整数
rootfs_size = 20480
cmd = 'resize.sh rootfs.img %dM' % rootfs_size
print('cmd         =', cmd)

# 也可以用 str.format 或 f-string（build.py 用的是 %）
print('f-string 版 =', f'resize.sh rootfs.img {rootfs_size}M')
