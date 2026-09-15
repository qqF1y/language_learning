#!/bin/bash
# 示例 12：ps / grep / awk / kill 清理进程
# 对应 build_rootfs 中的:
#   edge_cmd("build/scripts/kill_qemu.sh", out_path)
# kill_qemu.sh 内部:
#   for p in $(ps -aux | grep qemu-aarch64-static | grep -v grep | awk '{print $2}')
#   do kill -9 $p; done

echo "kill_qemu.sh 原逻辑:"
cat <<'EOF'
for p in $(ps -aux | grep qemu-aarch64-static | grep -v grep | awk -F' ' '{print $2}')
do
  kill -9 $p
done
EOF

echo
echo "管道拆解:"
echo "  ps -aux          列出所有进程"
echo "  grep xxx         过滤出包含 xxx 的行"
echo "  grep -v grep     排除 grep 命令自己"
echo "  awk '{print \$2}' 取第二列，即进程 PID"
echo "  kill -9 PID      强制结束进程"
echo

echo "安全演示（只打印，不真正 kill）:"
echo "当前 shell 的 PID 是 $$"
ps -ef | grep "$$" | grep -v grep | awk '{print "  PID=" $2}'

echo
echo "手动清理 qemu 的标准写法:"
echo "  pkill -9 qemu-aarch64-static   # 或按 PID: kill -9 <pid>"
