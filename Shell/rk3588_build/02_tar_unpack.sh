#!/bin/bash
# 示例 2：tar 解压
# 对应 build_rootfs 中的:
#   cmd = 'tar zxvf <rootfs>.tar.gz -C .'

echo "build_rootfs 原命令: tar zxvf rootfs/images/arm64/debian12-base.tar.gz -C ."
echo

workdir=$(mktemp -d /tmp/rk3588_tar_demo.XXXXXX)
cd "$workdir" || exit 1

# 先造一个示例目录用于打包
mkdir -p rootfs/etc rootfs/usr
echo "hello rockchip" > rootfs/etc/hostname
echo "1.0" > rootfs/version

echo "1) 打包：tar zcvf"
echo "   z=gzip 压缩  c=创建  v=显示过程  f=指定文件名"
tar zcvf rootfs.tar.gz rootfs

echo
echo "2) 删除原始目录，演示解压"
rm -rf rootfs

echo "3) 解包：tar zxvf ... -C ."
echo "   z=gzip  x=解包  v=显示过程  f=指定文件  -C .=解压到当前目录"
tar zxvf rootfs.tar.gz -C .

echo
echo "4) 解包后的内容:"
find rootfs -type f | sort

cd / || exit 1
rm -rf "$workdir"
