#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 11：封装执行 shell 命令的 edge_cmd

对应 build_rootfs 中的：
    cmd = 'rm -rf rootfs.img;mkdir -p rootfs'
    edge_cmd(cmd, out_path)
    if edge_cmd(cmd, out_path):
        EDGE_ERR('... failed')
        sys.exit(1)

要点：
- build.py 里的 edge_cmd 来自 utils.py，它封装了 os.system / subprocess.call。
- 返回值 0 表示命令成功，非 0 表示失败。
- path 参数非 None 时，用 cwd=path 让命令在指定目录里执行。
"""
import os
import subprocess


def edge_cmd(cmd, path):
    if cmd is None:
        print('[CMD] cmd 为 None，跳过')
        return 0
    print('[CMD]', cmd)
    if path is None:
        return os.system(cmd)
    else:
        # cwd=path：在指定目录下执行命令
        return subprocess.call(cmd, shell=True, cwd=path)


# 成功命令：返回 0
ret = edge_cmd('mkdir -p /tmp/rk3588_demo_dir', None)
print('mkdir 返回码:', ret)

# 失败命令：返回非 0
ret = edge_cmd('this_command_does_not_exist', None)
print('失败命令返回码:', ret)

# 在指定目录下执行（cwd）
edge_cmd('pwd', '/tmp/rk3588_demo_dir')

# 清理
edge_cmd('rm -rf /tmp/rk3588_demo_dir', None)
