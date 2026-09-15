#!/bin/bash
# 示例 7：mount / umount 挂载与卸载
# 对应 build_rootfs 中的:
#   cmd = 'sudo mount rootfs.img rootfs'
#   cmd = 'sudo umount rootfs'

echo "build_rootfs 原命令:"
echo "  sudo mount rootfs.img rootfs"
echo "  sudo umount rootfs"
echo

cat <<'EOF'
mount 把镜像文件或设备挂载到某个目录，之后就能像访问普通目录一样读写它：
  sudo mount -o loop rootfs.img rootfs
  # -o loop：用 loop 设备挂载普通文件（img 镜像）
卸载：
  sudo umount rootfs
EOF

echo

# 真实挂载需要 root 权限，非 root 时只讲解不执行
if [ "$(id -u)" -ne 0 ]; then
  echo "当前非 root，跳过真实挂载演示（mount/umount 需要 root）。"
  echo "实际构建时用 sudo 提权执行。"
  exit 0
fi

workdir=$(mktemp -d /tmp/rk3588_mount_demo.XXXXXX)
cd "$workdir" || exit 1

dd if=/dev/zero of=rootfs.img bs=1M count=8 status=none
mkfs.ext4 -q rootfs.img
mkdir -p rootfs

mount -o loop rootfs.img rootfs
echo "已挂载，df 信息:"
df -h | grep rootfs
umount rootfs
echo "已卸载"

cd / || exit 1
rm -rf "$workdir"
echo "挂载/卸载演示完成"
