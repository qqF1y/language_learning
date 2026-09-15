#!/bin/bash
# 示例 9：mkfs.ext4 格式化 ext4 文件系统
# 对应 build_rootfs 中的:
#   cmd = 'dd if=/dev/zero of=userdata.img bs=4K count=1K; sudo mkfs.ext4 userdata.img'

echo "build_rootfs 原命令:"
echo "  dd if=/dev/zero of=userdata.img bs=4K count=1K"
echo "  sudo mkfs.ext4 userdata.img"
echo
echo "mkfs.ext4 把 dd 生成的空文件格式化成 ext4 文件系统，才能被挂载使用。"
echo

# 真实格式化需要 root 权限
if [ "$(id -u)" -ne 0 ]; then
  echo "当前非 root，跳过真实格式化演示（mkfs.ext4 需要 root）。"
  exit 0
fi

workdir=$(mktemp -d /tmp/rk3588_mkfs_demo.XXXXXX)
cd "$workdir" || exit 1

dd if=/dev/zero of=userdata.img bs=4K count=1K status=none
echo "格式化前文件类型: $(file userdata.img)"
mkfs.ext4 -q userdata.img
echo "格式化后文件类型: $(file userdata.img | sed 's/, UUID.*//')"

cd / || exit 1
rm -rf "$workdir"
