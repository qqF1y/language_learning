#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
示例 13：build_rootfs 的 shell 命令与工具链

结论：build_rootfs 里 Python 只负责「拼命令 + 执行 + 判断结果」，
真正干活的是 shell 命令和工具链（tar / mount / e2fsck / resize2fs /
qemu-user-static / chroot / dd / mkfs.ext4 ...）。

本脚本用 dry-run（只打印、不真正执行）的方式，完整还原 build_rootfs
的命令序列。因为真实命令需要 root 权限、会挂载镜像并修改系统，
所以这里用 demo_cmd() 代替 edge_cmd()，只展示要执行什么。
"""


def demo_cmd(cmd, path=None):
    """模拟 build.py 的 edge_cmd()：打印命令，返回 0 表示成功。"""
    cwd = f"  (在目录 {path} 中)" if path is not None else ""
    print(f"$ {cmd}{cwd}")
    return 0


# ---- 用到的配置（来自 config.json，build_rootfs 里通过 conf['xxx'] 读取）----
root_path = "/home/qiurangfei/PRJ/a2614/rk3588_xenomai"
out_path = "/home/qiurangfei/PRJ/a2614/rk3588_xenomai/out/rk3588/rock5b/images"
arch = "arm64"
chip = "rk3588"
board = "rock5b"
rootfs_osname = "debian"
rootfs_version = "12"
rootfs_type = "debian-minimal"   # 简化为演示，实际是 debian/xxx
rootfs_size = "auto"
rootfs_user = "rock"

print("=== 1. 解包 rootfs 基础包（tar 解压） ===")
demo_cmd("rm -rf rootfs.img; mkdir -p rootfs", out_path)
demo_cmd(f"tar zxvf {root_path}/rootfs/images/{arch}/{rootfs_osname}{rootfs_version}-base.tar.gz -C .", out_path)

print("\n=== 2. 调整 rootfs.img 大小（resize.sh -> e2fsck + resize2fs） ===")
if rootfs_size == "auto":
    demo_cmd(f"{root_path}/build/scripts/resize.sh {out_path}/rootfs.img 20480M")
else:
    demo_cmd(f"{root_path}/build/scripts/resize.sh {out_path}/rootfs.img {rootfs_size}M")

print("\n=== 3. 挂载镜像（mount） ===")
demo_cmd("sudo mount rootfs.img rootfs", out_path)

print("\n=== 4. 放入 qemu 用户态模拟器（x86 主机跑 ARM 二进制） ===")
qemu_bin = f"{root_path}/build/scripts/qemu-aarch64-static"
demo_cmd(f"sudo cp {qemu_bin} rootfs/usr/bin", out_path)

print("\n=== 5. 拷贝预装文件（cp） ===")
demo_cmd(f"sudo cp -rf {root_path}/vendor/common/pre-install {out_path}/rootfs/")
demo_cmd(f"sudo cp -rf {root_path}/rootfs/{rootfs_osname}/packages-local {out_path}/rootfs/usr/share/")
demo_cmd(f"sudo cp -rf {root_path}/vendor/{chip}/{board}/pre-install/* {out_path}/rootfs/pre-install/")
demo_cmd(f"sudo mkdir -p etc/chroot; echo \"rockchip,{chip}-chroot\" | sudo tee etc/chroot/compatible", f"{out_path}/rootfs")

print("\n=== 6. chroot 进 rootfs 执行安装脚本 ===")
demo_cmd(
    f"{root_path}/build/scripts/rootfs-install.sh {rootfs_type} {rootfs_user} ... {rootfs_osname} ...",
    out_path,
)
print("    └─ rootfs-install.sh 内部：mount proc/sys/dev -> chroot rootfs /pre-install/install.sh -> umount")

print("\n=== 7. 清理（rm 删除临时文件/日志/历史） ===")
demo_cmd(f"sudo rm -rf pre-install var/log/* root/.bash_history home/{rootfs_user}/.bash_history etc/chroot", f"{out_path}/rootfs")

print("\n=== 8. 计算实际大小（du -m | awk） ===")
demo_cmd(f"sudo {root_path}/build/scripts/getsize.sh {out_path}/rootfs")

print("\n=== 9. 收尾：杀 qemu 进程 -> 卸载 -> 删除挂载目录 ===")
demo_cmd(f"{root_path}/build/scripts/kill_qemu.sh", out_path)
demo_cmd("sudo umount rootfs/dev", out_path)
demo_cmd("sudo umount rootfs", out_path)
demo_cmd("rm -rf rootfs", out_path)

print("\n=== 10. 按实际大小自动收缩镜像 ===")
demo_cmd(f"{root_path}/build/scripts/resize.sh {out_path}/rootfs.img 224M")

print("\n=== 11. 生成 userdata 分区镜像（dd + mkfs.ext4） ===")
demo_cmd("dd if=/dev/zero of=userdata.img bs=4K count=1K; sudo mkfs.ext4 userdata.img", out_path)

print("\n工具链小结：")
print("  coreutils        : rm / mkdir / cp / dd / echo / tee / du")
print("  tar              : 解压 rootfs 基础包")
print("  util-linux       : mount / umount")
print("  e2fsprogs        : e2fsck / resize2fs / mkfs.ext4")
print("  qemu-user-static : qemu-aarch64-static（跨架构 chroot）")
print("  chroot           : 切换根目录执行安装脚本")
print("  build/scripts/*  : resize.sh / rootfs-install.sh / getsize.sh / kill_qemu.sh")
