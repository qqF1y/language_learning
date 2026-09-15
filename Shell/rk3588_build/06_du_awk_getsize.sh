#!/bin/bash
# 示例 6：du / awk 计算目录大小
# 对应 build_rootfs 中的:
#   msize = int(edge_cmd_result('sudo .../getsize.sh out/.../rootfs'))
# getsize.sh 内部就是: du -m -d 0 <path> | awk '{print $1}'

echo "build_rootfs 里 getsize.sh 的内容:"
echo "  size=\$(du -m -d 0 \$1 | awk -F' ' '{print \$1}')"
echo

workdir=$(mktemp -d /tmp/rk3588_du_demo.XXXXXX)
cd "$workdir" || exit 1

mkdir -p rootfs
# 生成一个 5M 的文件
dd if=/dev/zero of=rootfs/big.bin bs=1M count=5 status=none

echo "1) du -m -d 0：只统计目录本身大小，单位 M"
du -m -d 0 rootfs

echo "2) 用 awk 只取第一列（大小数字）"
size=$(du -m -d 0 rootfs | awk '{print $1}')
echo "   size=${size}M"

echo
echo "对应 Python: msize = int(edge_cmd_result(...)) 会得到 ${size}"
echo "然后 build_rootfs 里做: msize = msize + msize / 10"

cd / || exit 1
rm -rf "$workdir"
