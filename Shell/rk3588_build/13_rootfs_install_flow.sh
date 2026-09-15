#!/bin/bash
# 示例 13：汇总——build_rootfs 完整 shell 流程（dry-run）
# 用 run() 模拟 build.py 里的 edge_cmd(cmd, out_path)，
# 只打印命令、不真正执行危险操作。

run() {
  echo "\$ $*"
}

echo "=== build_rootfs 完整 shell 流程（dry-run，只打印不执行）==="
echo

run rm -rf rootfs.img
run mkdir -p rootfs
run tar zxvf rootfs/images/arm64/debian12-base.tar.gz -C .
run ./build/scripts/resize.sh out/rk3588/rock5b/images/rootfs.img 20480M
run sudo mount rootfs.img rootfs
run sudo cp build/scripts/qemu-aarch64-static rootfs/usr/bin
run sudo cp -rf vendor/common/pre-install out/.../rootfs/
run sudo cp -rf rootfs/debian/packages-local out/.../rootfs/usr/share/
run sudo cp -rf vendor/rk3588/rock5b/pre-install/\* out/.../rootfs/pre-install/
run "sudo mkdir -p etc/chroot; echo rockchip,rk3588-chroot | sudo tee etc/chroot/compatible"
run ./build/scripts/rootfs-install.sh debian rock ... debian ...
run sudo rm -rf pre-install var/log/\* root/.bash_history
run ./build/scripts/getsize.sh out/.../rootfs
run ./build/scripts/kill_qemu.sh
run sudo umount rootfs/dev
run sudo umount rootfs
run rm -rf rootfs
run ./build/scripts/resize.sh out/.../rootfs.img 224M
run "dd if=/dev/zero of=userdata.img bs=4K count=1K; sudo mkfs.ext4 userdata.img"

echo
echo "在真实 build.py 中，上面每一行 run 对应一次:"
echo "  edge_cmd(cmd, out_path)  或  edge_cmd(cmd, out_path/rootfs)"
echo "Python 只负责拼这些字符串、执行、按返回码判断失败。"
