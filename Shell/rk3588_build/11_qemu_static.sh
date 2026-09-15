#!/bin/bash
# 示例 11：qemu-aarch64-static 跨架构模拟器
# 对应 build_rootfs 中的:
#   qemu_bin = '%s/build/scripts/qemu-%s-static' % (root_path, arch_alias)
#   cmd = 'sudo cp %s rootfs/usr/bin' % qemu_bin

qemu=/home/qiurangfei/PRJ/a2614/rk3588_xenomai/build/scripts/qemu-aarch64-static

echo "build_rootfs 原命令:"
echo "  sudo cp $qemu rootfs/usr/bin"
echo

echo "作用：qemu-user-static 是用户态跨架构模拟器，"
echo "让 x86 宿主机能运行 ARM（aarch64）的二进制程序。"
echo

if [ -x "$qemu" ]; then
  echo "找到 qemu-aarch64-static，文件类型:"
  file "$qemu"
  echo
  "$qemu" --version 2>/dev/null | head -3 || echo "(该版本不支持 --version)"
else
  echo "未找到 $qemu"
  echo "实际路径在 build/scripts/ 下，也有 qemu-arm-static（对应 32 位 arm）。"
fi

echo
echo "为什么 chroot 前要先 cp 进 rootfs/usr/bin？"
echo "  chroot 进 ARM rootfs 后，执行 /pre-install/install.sh 时，"
echo "  内核 binfmt 会在 rootfs/usr/bin 里找 qemu-aarch64-static 来解释 ARM 程序。"
echo
echo "收尾时 build_rootfs 会调用 kill_qemu.sh 清理残留的 qemu 进程（见示例 12）。"
