#!/bin/bash
# 示例 11：qemu-aarch64-static 在 x86 宿主机上真实执行 ARM 程序
#
# 对应 build_rootfs 中的:
#   qemu_bin = '%s/build/scripts/qemu-%s-static' % (root_path, arch_alias)
#   cmd = 'sudo cp %s rootfs/usr/bin' % qemu_bin
#
# 本脚本演示 qemu-user 的核心作用：让 x86 CPU 运行 aarch64 二进制。
# 全程无需 root、无需 mount：用 debugfs 从 ext4 镜像里取出 ARM 程序，
# 再用 qemu-aarch64-static 显式执行。

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

echo "3) file 验证它是 ARM aarch64 程序:"
file "$WORK/minrootfs/bin/dash"

echo
echo "4) 用 qemu-aarch64-static 显式执行 ARM dash:"
"$QEMU" -L "$WORK/minrootfs" "$WORK/minrootfs/bin/dash" -c 'echo "  hello from ARM64 dash"; pwd'

echo
echo "5) 直接执行会失败——因为宿主机 /lib 下没有 ARM 动态库:"
"$WORK/minrootfs/bin/dash" -c 'echo ok' 2>&1 | sed 's/^/  /'

echo
echo "结论:"
echo "  - qemu 能跑 ARM 程序，但必须找到 ARM 的动态库。"
echo "  - 用 -L 指定库根目录只是权宜之计。"
echo "  - build_rootfs 的做法：把 qemu 拷进完整 rootfs，再 chroot 进去，"
echo "    这样 /lib/ld-linux-aarch64.so.1 就是 ARM rootfs 自己的库（见 10_chroot.sh）。"
echo
echo "缓存镜像保留在 $CACHE（不需要可 rm -rf 清理）"
