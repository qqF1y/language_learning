#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 8：input() 读取用户输入

对应 build_rootfs 中的：
    ret = input('\\033[32m[EDGE INFO] Rootfs already exists, do you want to continue?[Y/n] \\033[0m')
    if ret == 'N' or ret == 'n':
        ...
    elif ret == 'Y' or ret == 'y':
        ...

要点：
- input(提示语) 会等待用户输入，返回用户键入的字符串。
- 返回结果通常要判断大小写，例如 y / n。
"""
ret = input('rootfs.img 已存在，是否继续？[Y/n] ')

if ret == 'N' or ret == 'n':
    print('删除旧镜像，重新构建')
elif ret == 'Y' or ret == 'y':
    print('继续使用已有镜像')
else:
    print('输入无效，请重新运行')
