#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 2：字典（dict）取值

对应 build_rootfs 中的：
    conf = self.config.get()
    out_path = conf['out_path']
    arch = conf['arch']
    chip = conf['chip']

要点：
- dict 是「键 -> 值」的映射，用 conf['键名'] 取值。
- 键不存在会抛 KeyError；用 conf.get('键名', 默认值) 更安全。
- config.get() 返回的 conf 就是一个 dict。
"""

conf = {
    'out_path': '/out/rk3588/rock5b/images',
    'arch': 'arm64',
    'chip': 'rk3588',
    'board': 'rock5b',
}

# 方式一：中括号取值
out_path = conf['out_path']
arch = conf['arch']
print('out_path =', out_path)
print('arch     =', arch)

# 方式二：get 取值，键不存在时返回默认值
board = conf.get('board', 'unknown')
rootfs_osname = conf.get('rootfs_osname', 'debian')
print('board          =', board)
print('rootfs_osname  =', rootfs_osname)
