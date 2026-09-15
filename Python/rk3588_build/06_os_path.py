#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 6：os.path 判断文件/目录

对应 build_rootfs 中的：
    if os.path.exists(chroot_bin) == False:
        ...
    if os.path.isfile(rootfs_taskrel):
        ...

要点：
- os.path.exists(path)  路径（文件或目录）是否存在。
- os.path.isfile(path)  是否是一个普通文件。
- os.path.isdir(path)   是否是一个目录。
- 使用前需 import os。
"""
import os
import tempfile

chroot_bin = '/usr/sbin/chroot'
print('chroot 是否存在:', os.path.exists(chroot_bin))

# 创建临时文件验证 isfile
tmp = os.path.join(tempfile.gettempdir(), 'rk3588_demo.txt')
with open(tmp, 'w') as f:
    f.write('hello')

print('临时文件是文件吗:', os.path.isfile(tmp))
print('临时目录是目录吗:', os.path.isdir(tempfile.gettempdir()))

# 清理
os.remove(tmp)
