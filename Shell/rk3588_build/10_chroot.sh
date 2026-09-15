#!/bin/bash
# 示例 10：chroot 切换根目录，跨架构运行 ARM rootfs
#
# 对应 build_rootfs 中的:
#   cmd = 'build/scripts/rootfs-install.sh ...'
# rootfs-install.sh 内部就是: 挂载 proc/sys/dev -> chroot -> 卸载
#
# chroot 需要 root 权限（CAP_SYS_CHROOT），所以:
#   - 非 root 运行：打印完整流程和命令，不真实执行
#   - root 运行  ：真实 mount + chroot 进入 ARM rootfs 执行命令

set -u

ROOTFS_IMG=/home/qiurangfei/PRJ/a2614/rk3588_xenomai/rootfs/rootfs.img
QEMU=/home/qiurangfei/PRJ/a2614/rk3588_xenomai/build/scripts/qemu-aarch64-static
CACHE=/tmp/rk3588_qemu_cache

echo "chroot：把某个目录当作新的 / 根目录，在里面执行命令。"
echo "这样操作的就是目标系统（ARM rootfs），而不是宿主机。"
echo

cat <<'EOF'
build_rootfs 里 rootfs-install.sh 的真实流程：
  1) 挂载宿主机特殊目录到 rootfs（chroot 后还要能访问内核/设备/网络）:
     sudo mount -t proc /proc rootfs/proc/
     sudo mount -t sysfs /sys rootfs/sys/
     sudo mount --rbind /dev rootfs/dev/
     sudo mount --bind /etc/resolv.conf rootfs/etc/resolv.conf

  2) 跨架构时先放入 qemu，否则无法执行 ARM 的安装脚本:
     sudo cp build/scripts/qemu-aarch64-static rootfs/usr/bin/

  3) 切换根目录并执行安装脚本:
     sudo chroot rootfs /pre-install/install.sh <参数>

  4) 安装完反向卸载:
     sudo umount rootfs/proc
     sudo umount rootfs/sys
     sudo mount --make-rslave rootfs/dev
     sudo umount -R rootfs/dev
     sudo umount rootfs/etc/resolv.conf
EOF

echo

if [ "$(id -u)" -ne 0 ]; then
  echo "当前非 root，chroot 需要 root 权限，无法真实演示。"
  echo "想真实执行，请用:  sudo bash $0"
  exit 0
fi

# ==================== root 分支：真实演示 ====================

# 用解压出的独立副本（320MB），不碰项目 7.1G 的 rootfs.img
if [ ! -f "$CACHE/rootfs.img" ]; then
  BASE_TAR=/home/qiurangfei/PRJ/a2614/rk3588_xenomai/rootfs/images/arm64/debian11-base.tar.gz
  echo "首次运行，解压 rootfs 包..."
  mkdir -p "$CACHE"
  tar zxf "$BASE_TAR" -C "$CACHE"
fi
IMG="$CACHE/rootfs.img"

WORK=$(mktemp -d /tmp/rk3588_chroot_demo.XXXXXX)
cleanup() {
  umount "$WORK/rootfs/proc" 2>/dev/null
  umount "$WORK/rootfs/sys" 2>/dev/null
  umount "$WORK/rootfs/dev" 2>/dev/null
  umount "$WORK/rootfs/etc/resolv.conf" 2>/dev/null
  umount "$WORK/rootfs" 2>/dev/null
  rm -rf "$WORK"
}
trap cleanup EXIT

mkdir -p "$WORK/rootfs"

echo "1) 挂载 rootfs.img（loop 挂载）"
mount -o loop "$IMG" "$WORK/rootfs"

echo "2) 放入 qemu（对应 build_rootfs）"
cp "$QEMU" "$WORK/rootfs/usr/bin/"

echo "3) 挂载 proc/sys/dev/resolv.conf（对应 rootfs-install.sh 的 pre_install）"
mount -t proc /proc "$WORK/rootfs/proc"
mount -t sysfs /sys "$WORK/rootfs/sys"
mount --rbind /dev "$WORK/rootfs/dev"
mount --bind /etc/resolv.conf "$WORK/rootfs/etc/resolv.conf"

echo "4) chroot 进入 ARM rootfs 执行命令"
chroot "$WORK/rootfs" /bin/sh -c 'echo "    进入 rootfs，运行 /bin/sh 成功"'

echo "5) 卸载由 cleanup 自动完成"
