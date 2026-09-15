#!/bin/bash
set -u


QEMU=/home/qiurangfei/PRJ/a2614/rk3588_xenomai/build/scripts/qemu-aarch64-static
BASE_TAR=/home/qiurangfei/PRJ/a2614/rk3588_xenomai/rootfs/images/arm64/debian11-base.tar.gz
CACHE=/tmp/rk3588_qemu_cache

[ -e "$QEMU" ] || { echo "缺少 qemu: $QEMU" >&2; exit 1; }
[ -e "$BASE_TAR" ] || { echo "缺少 rootfs 包: $BASE_TAR" >&2; exit 1; }

WORK=$(mktemp -d /tmp/rk3588_qemu_demo.XXXXXX)
trap 'rm -rf "$WORK"' EXIT

echo "1) 准备 ARM rootfs 镜像（解压 debian11-base.tar.gz，里面就是 rootfs.img）"
mkdir -p "$CACHE"
if [ ! -f "$CACHE/rootfs.img" ]; then
  echo "   首次运行，正在解压（约 320MB）..."
  tar zxf "$BASE_TAR" -C "$CACHE"
fi
IMG="$CACHE/rootfs.img"
echo "   镜像: $IMG ($(du -h "$IMG" | awk '{print $1}'))"

echo
echo "2) 用 debugfs 从镜像里提取 ARM 程序（dash + libc + ld-linux，无需 root/挂载）"
mkdir -p "$WORK/minrootfs/bin" "$WORK/minrootfs/lib"
debugfs -R "dump /usr/bin/dash $WORK/minrootfs/bin/dash" "$IMG" >/dev/null 2>&1
debugfs -R "dump /usr/lib/aarch64-linux-gnu/ld-2.31.so $WORK/minrootfs/lib/ld-linux-aarch64.so.1" "$IMG" >/dev/null 2>&1
debugfs -R "dump /usr/lib/aarch64-linux-gnu/libc-2.31.so $WORK/minrootfs/lib/libc.so.6" "$IMG" >/dev/null 2>&1
chmod +x "$WORK/minrootfs/bin/dash"