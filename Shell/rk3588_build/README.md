# build_rootfs 用到的 shell 命令与工具示例

对应 `/home/qiurangfei/PRJ/a2614/rk3588_xenomai/build/python3/build.py` 中
`Build.build_rootfs()`（第 555~689 行）用到的 shell 命令和工具链。

`build_rootfs` 的 Python 只是拼命令、执行、判断结果，真正干活的是这些工具：

| 脚本 | 命令/工具 | build_rootfs 中的用途 |
| --- | --- | --- |
| `01_coreutils_rm_mkdir.sh` | `rm` `mkdir` | 清理旧镜像、创建挂载目录 |
| `02_tar_unpack.sh` | `tar` | 解压 rootfs 基础包 |
| `03_cp_copy.sh` | `cp -rf` | 拷贝 pre-install / packages-local |
| `04_echo_tee.sh` | `echo` `tee` | 写入 chroot compatible 标记 |
| `05_dd_create_image.sh` | `dd` | 生成 userdata.img 空镜像 |
| `06_du_awk_getsize.sh` | `du` `awk` | 计算 rootfs 实际大小（getsize.sh） |
| `07_mount_umount.sh` | `mount` `umount` | 挂载/卸载 rootfs.img |
| `08_e2fsck_resize2fs.sh` | `e2fsck` `resize2fs` | 检查、调整 ext4 镜像大小（resize.sh） |
| `09_mkfs_ext4.sh` | `mkfs.ext4` | 格式化 userdata 为 ext4 |
| `10_chroot.sh` | `chroot` | 真实 mount + chroot 进入 ARM rootfs（需 root） |
| `11_qemu_static.sh` | `qemu-aarch64-static` | 真实执行 ARM 二进制（debugfs 提取，无需 root） |
| `12_ps_grep_kill.sh` | `ps` `grep` `awk` `kill` | 清理残留 qemu 进程（kill_qemu.sh） |
| `13_rootfs_install_flow.sh` | 全部汇总 | dry-run 完整还原 build_rootfs 流程 |

## 运行方式

所有脚本都可独立运行：

```bash
cd /home/qiurangfei/PRJ/language_learning/Shell/rk3588_build
bash 01_coreutils_rm_mkdir.sh
```

需要 root 权限的脚本（`07/08/09/10`）在非 root 环境下会自动跳过真实操作，
只打印命令讲解和实际构建时的用法；在 root 环境下会做真实演示。
