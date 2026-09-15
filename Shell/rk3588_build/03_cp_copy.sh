#!/bin/bash
# 示例 3：cp 拷贝
# 对应 build_rootfs 中的:
#   cmd = 'sudo cp -rf vendor/common/pre-install rootfs/'

echo "build_rootfs 原命令: sudo cp -rf vendor/common/pre-install out/.../rootfs/"
echo

workdir=$(mktemp -d /tmp/rk3588_cp_demo.XXXXXX)
cd "$workdir" || exit 1

mkdir -p src/sub dest
echo "a" > src/a.txt
echo "b" > src/sub/b.txt

echo "1) cp -r：递归拷贝整个目录到 dest/"
cp -r src dest/
echo "   拷贝后的文件:"
find dest -type f | sort

echo "2) cp -f：覆盖已存在文件，不提示"
echo "new" > dest/src/a.txt
cp -f src/a.txt dest/src/a.txt
echo "   覆盖后 dest/src/a.txt 内容: $(cat dest/src/a.txt)"

echo "3) cp 通配符：一次拷贝多个文件"
mkdir -p out
cp src/*.txt out/
echo "   out/ 下文件: $(ls out/)"

cd / || exit 1
rm -rf "$workdir"
