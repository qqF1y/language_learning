#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 12：算术运算

对应 build_rootfs 中的：
    msize = int(edge_cmd_result('sudo ... getsize.sh ...'))
    msize = msize + msize / 10
    cmd = '... %dM' % msize

要点：
- + 加法，- 减法，* 乘法，/ 除法（Python3 结果为浮点数）。
- msize = msize + msize / 10 表示在原来基础上增加十分之一。
- 用 %d 格式化时通常要 int() 取整。
"""

msize = 200  # 单位 M
print('原始大小:', msize)

# 加法
msize = msize + 4
print('+4 后:', msize)

# 除法与累加：增加原来的 1/10
msize = msize + msize / 10
print('再增加 1/10 后:', msize, '类型:', type(msize).__name__)

# %d 格式化时取整
print('resize.sh rootfs.img %dM' % int(msize))
