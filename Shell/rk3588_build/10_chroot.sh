#!/bin/bash
# 示例 10：chroot 切换根目录
# 对应 build_rootfs 中的:
#   cmd = 'build/scripts/rootfs-install.sh ...'
# rootfs-install.sh 内部就是挂载 proc/sys/dev 后执行:
#   sudo chroot rootfs /pre-install/install.sh <参数>

echo "rootfs-install.sh 的真实流程:"
cat <<'EOF'
1) 挂载宿主机的特殊目录到 rootfs（chroot 后还要能访问内核/设备）:
   sudo mount -t proc /proc rootfs/proc/
   sudo mount -t sysfs /sys rootfs/sys/
   sudo mount --rbind /dev rootfs/dev/
   sudo mount --bind /etc/resolv.conf rootfs/etc/resolv.conf

2) 切换根目录并执行安装脚本:
   sudo chroot rootfs /pre-install/install.sh <rootfs_type> <user> ...

3) 安装完反向卸载:
   sudo umount rootfs/proc
   sudo umount rootfs/sys
   sudo mount --make-rslave rootfs/dev
   sudo umount -R rootfs/dev
   sudo umount rootfs/etc/resolv.conf
EOF

echo
echo "chroot 的作用：把 rootfs 当作新的根目录，在里面跑命令，"
echo "这样安装脚本操作的就是目标系统（ARM rootfs），而不是宿主机。"
echo
echo "注意：跨架构时，chroot 前必须先放入 qemu-aarch64-static（见示例 11），"
echo "否则无法执行 ARM 的 /pre-install/install.sh。"

# 真实 chroot 需要一个完整的 rootfs，这里只做讲解，不执行危险操作
exit 0
