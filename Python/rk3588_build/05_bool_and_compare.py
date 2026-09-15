#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 5：布尔值 True/False 与比较、逻辑运算

对应 build_rootfs 中的：
    build_new = True
    if os.path.exists(...) == True:
    if ret == 'N' or ret == 'n':

要点：
- 布尔值只有 True / False（首字母大写）。
- == 判断相等，!= 判断不相等。
- and：两边都成立才成立；or：任一边成立即成立。
- os.path.exists(...) 这类函数返回布尔值，可参与 if 判断。
"""
import os

# 布尔变量
build_new = True
print('build_new =', build_new)

# 比较运算返回布尔值
path = '/tmp/rk3588_example_file.txt'
exists = os.path.exists(path)
print('文件是否存在?', exists)

# 逻辑运算 or
ret = 'n'
if ret == 'N' or ret == 'n':
    print("用户选择 '否'，将重新构建")

# build_rootfs 中常见写法：显式与 True/False 比较
if exists == False:
    print('该文件不存在，可以开始构建')
else:
    print('该文件已存在')
