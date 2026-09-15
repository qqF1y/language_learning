#!/bin/bash
echo "build_rootfs"

workdir=$(mktemp -d /tmp/rk3588_build_rootfs.XXXXXX)
cd "$workdir" || exit 1

echo "1) mkdir"
mkdir -p a/b/c
echo "   已创建 a/b/c"

echo "2) "

cd / || exit 1
rm -rf "$workdir"
echo
echo "完成，已清理临时目录 $workdir"