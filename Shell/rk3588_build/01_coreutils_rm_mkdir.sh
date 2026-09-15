#!/bin/bash
# 示例 1：rm / mkdir（coreutils 基础命令）
# 对应 build_rootfs 中的:
#   cmd = 'rm -rf rootfs.img; mkdir -p rootfs'

echo "build_rootfs 原命令: rm -rf rootfs.img; mkdir -p rootfs"
echo

workdir=$(mktemp -d /tmp/rk3588_rm_mkdir.XXXXXX)
cd "$workdir" || exit 1

echo "1) mkdir -p：递归创建多级目录，目录已存在也不报错"
mkdir -p a/b/c
echo "   已创建 a/b/c"

echo "2) 造一个文件用于演示 rm"
touch a/old.img
echo "   a/old.img 是否存在: $([ -f a/old.img ] && echo yes || echo no)"

echo "3) rm -f：强制删除文件（不提示确认）"
rm -f a/old.img
echo "   删除后是否存在: $([ -f a/old.img ] && echo yes || echo no)"

echo "4) rm -rf：递归强制删除目录（危险，注意路径）"
rm -rf a
echo "   a 是否存在: $([ -d a ] && echo yes || echo no)"

cd / || exit 1
rm -rf "$workdir"
echo
echo "完成，已清理临时目录 $workdir"
