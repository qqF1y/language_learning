#!/bin/bash
# 示例 4：echo / tee / 管道 / 重定向
# 对应 build_rootfs 中的:
#   cmd = 'sudo mkdir -p etc/chroot; echo "rockchip,<chip>-chroot" | sudo tee etc/chroot/compatible'

echo "build_rootfs 原命令: echo \"rockchip,rk3588-chroot\" | sudo tee etc/chroot/compatible"
echo

workdir=$(mktemp -d /tmp/rk3588_echo_demo.XXXXXX)
cd "$workdir" || exit 1

echo "1) echo：输出字符串"
echo "rockchip,rk3588-chroot"

echo "2) > 重定向写文件（覆盖）"
echo "line1" > file.txt
echo "   文件内容: $(cat file.txt)"

echo "3) >> 重定向追加"
echo "line2" >> file.txt
echo "   文件内容: $(cat file.txt)"

echo "4) | tee：管道把前一个命令的输出，同时写到屏幕和文件"
echo "compatible info" | tee file.txt
echo "   文件内容: $(cat file.txt)"

echo "5) tee -a：追加写而不是覆盖"
echo "append" | tee -a file.txt
echo "   文件内容: $(cat file.txt)"

cd / || exit 1
rm -rf "$workdir"
