#!/bin/bash
# 示例 8：e2fsck / resize2fs 检查与调整 ext4 镜像
# 对应 build_rootfs 中的:
#   cmd = '%s/build/scripts/resize.sh %s/rootfs.img 20480M'
# resize.sh 内部就是先 e2fsck 检查，再 resize2fs 调整大小

echo "resize.sh 内部做的事:"
cat <<'EOF'
1) sudo e2fsck -fy rootfs.img        # 检查并自动修复 ext 文件系统
2) sudo resize2fs -f rootfs.img 20480M   # 把文件系统调整到 20480M
EOF

echo
echo "对应 build_rootfs 命令:"
echo "  ./build/scripts/resize.sh out/.../rootfs.img 20480M"
echo

# 真实操作需要 root 权限
if [ "$(id -u)" -ne 0 ]; then
  echo "当前非 root，跳过真实演示（e2fsck/resize2fs 需要 root）。"
  exit 0
fi

workdir=$(mktemp -d /tmp/rk3588_resize_demo.XXXXXX)
cd "$workdir" || exit 1

dd if=/dev/zero of=rootfs.img bs=1M count=16 status=none
mkfs.ext4 -q rootfs.img

echo "原始大小: $(du -m rootfs.img | awk '{print $1}')M"
e2fsck -fy rootfs.img >/dev/null
resize2fs -f rootfs.img 32M
echo "调整后大小: $(du -m rootfs.img | awk '{print $1}')M"

cd / || exit 1
rm -rf "$workdir"
