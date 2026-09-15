# build_rootfs 语法学习示例

本目录对应 `/home/qiurangfei/PRJ/a2614/rk3588_xenomai/build/python3/build.py`
中 `Build.build_rootfs()` 方法（第 555 ~ 689 行）用到的 Python 语法。

`build_rootfs` 主要做了这些事：读取配置、判断架构、解压/挂载 rootfs、
拷贝预装文件、调用安装脚本、卸载并收缩镜像。它用到的语法知识点如下表：

| 示例文件 | 语法知识点 | build_rootfs 中的对应代码 |
| --- | --- | --- |
| `01_class_and_method.py` | 类、实例方法、`self` | `def build_rootfs(self):` |
| `02_dict_access.py` | 字典取值 | `conf['out_path']`、`conf['arch']` |
| `03_string_format.py` | `%` 字符串格式化（`%s`/`%d`） | `'%s/vendor/%s/%s' % (...)` |
| `04_if_elif_else.py` | if / elif / else 分支 | `if arch == 'arm64': ... else: ...` |
| `05_bool_and_compare.py` | 布尔值、`==`/`!=`、`or` | `build_new = True`、`ret == 'N' or ret == 'n'` |
| `06_os_path.py` | `os.path.exists` / `os.path.isfile` | `os.path.exists(chroot_bin)` |
| `07_sys_exit.py` | `sys.exit(1)` 异常退出 | `sys.exit(1)` |
| `08_input.py` | `input()` 交互输入 | `ret = input('... [Y/n] ...')` |
| `09_string_escape.py` | 转义字符 `\n`、ANSI 颜色 `\033[32m` | 提示文案 |
| `10_split_and_int.py` | `str.split()`、`int()` 类型转换 | `rootfs_type.split('-')[0]`、`int(rootfs_size)` |
| `11_run_shell_cmd.py` | 封装执行 shell 命令（`edge_cmd`） | `edge_cmd(cmd, out_path)` |
| `12_arithmetic.py` | 算术运算 `+`、`/` | `msize + msize / 10` |

## 运行方式

所有示例都是独立的 Python 脚本，不需要依赖项目里的 `utils.py` / `config.py`。
直接运行即可：

```bash
cd /home/qiurangfei/PRJ/language_learning/Python/rk3588_build
python3 01_class_and_method.py
```

其中 `08_input.py` 需要交互输入，运行时按提示键入 `y` / `n` 即可。

## build_rootfs 里的 shell 命令与工具链

你的观察是对的：`build_rootfs` 里 Python 只做「编排」——
拼命令字符串、执行、按返回码判断、控制流程。真正的活全是 shell 命令和各种工具干的。

Python 的三件事：

1. **拼命令**：用 `%` 把路径、参数填进命令模板，例如
   `cmd = 'sudo mount rootfs.img rootfs'`
2. **执行命令**：调用 `edge_cmd(cmd, out_path)`（内部是 `os.system` / `subprocess.call(cwd=...)`）
3. **判断结果**：`if edge_cmd(cmd, out_path):` 返回非 0 就报错 `sys.exit(1)`

build_rootfs 的完整流程与对应命令/工具：

| 阶段 | Python 里的命令（模板） | 用到的工具 |
| --- | --- | --- |
| 解包 rootfs | `rm -rf rootfs.img; mkdir -p rootfs` | coreutils：`rm` `mkdir` |
| | `tar zxvf <rootfs>.tar.gz -C .` | `tar`（gzip 解压） |
| 调整镜像大小 | `build/scripts/resize.sh rootfs.img 20480M` | `resize.sh` → `e2fsck` + `resize2fs`（e2fsprogs） |
| 挂载镜像 | `sudo mount rootfs.img rootfs` | `mount`（util-linux）、`sudo` |
| 放入 ARM 模拟器 | `sudo cp build/scripts/qemu-aarch64-static rootfs/usr/bin` | `qemu-user-static` |
| 拷贝预装文件 | `sudo cp -rf vendor/common/pre-install rootfs/` | `cp` |
| | `sudo cp -rf rootfs/<os>/packages-local rootfs/usr/share/` | `cp` |
| | `sudo cp -rf vendor/<chip>/<board>/pre-install/* rootfs/pre-install/` | `cp` |
| | `sudo mkdir -p etc/chroot; echo "rockchip,<chip>-chroot" \| sudo tee etc/chroot/compatible` | `mkdir` `echo` `tee` |
| chroot 安装 | `build/scripts/rootfs-install.sh ...` | `rootfs-install.sh` → `mount`/`umount` + `chroot` → `/pre-install/install.sh` |
| 清理 | `sudo rm -rf pre-install var/log/* ... etc/chroot` | `rm` |
| 计算大小 | `getsize.sh rootfs`（`du -m -d 0 ... | awk`） | `du` `awk` |
| 结束 qemu | `build/scripts/kill_qemu.sh` | `ps` `grep` `awk` `kill` |
| 卸载 | `sudo umount rootfs/dev`、`sudo umount rootfs`、`rm -rf rootfs` | `umount` `rm` |
| 自动收缩 | `build/scripts/resize.sh rootfs.img <msize>M` | `e2fsck` + `resize2fs` |
| 生成 userdata | `dd if=/dev/zero of=userdata.img bs=4K count=1K; sudo mkfs.ext4 userdata.img` | `dd` `mkfs.ext4` |

关键点：`build/scripts/` 下的脚本本质也是 shell，例如
`rootfs-install.sh` 就是「挂载 proc/sys/dev → `chroot` 进 rootfs 跑 `/pre-install/install.sh` → 卸载」，
真正的软件包安装逻辑在 rootfs 里的 `pre-install/install.sh`。

所以要看懂 `build_rootfs`，除了 Python 语法，还需要认识这些 **Linux 基础命令** 和
**e2fsprogs / qemu-user-static / chroot** 等工具。`13_shell_cmd_and_tools.py` 用 dry-run
方式完整演示了这个「Python 拼命令 → 工具干活」的过程。
