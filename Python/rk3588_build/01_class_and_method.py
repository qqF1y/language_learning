#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 1：类与实例方法

对应 build_rootfs 中的：
    def build_rootfs(self):
        conf = self.config.get()
        root_path = self.root_path
        ...

要点：
- class 定义一个类。
- __init__ 是构造方法，在创建对象时自动调用，用来保存实例属性。
- 方法的第一个参数固定是 self，代表当前对象本身。
- 方法内部通过 self.xxx 访问实例属性或调用其他方法。
"""


class Build:
    def __init__(self, root_path):
        # 实例属性：每个对象各自保存一份
        self.root_path = root_path

    def build_rootfs(self):
        # 通过 self 读取实例属性
        print("开始构建 rootfs，根路径是:", self.root_path)


if __name__ == '__main__':
    # 创建对象时，root_path 会传给 __init__
    build = Build("/home/qiurangfei/PRJ/a2614/rk3588_xenomai")
    build.build_rootfs()
