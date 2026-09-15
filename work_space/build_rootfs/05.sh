echo "build_rootfs 原命令: dd if=/dev/zero of=userdata.img bs=4K count=1K"


workdir=$(mktemp -d /tmp/rk3588_dd_demo.XXXXXX)
cd "$workdir" || exit 1

echo "dd 参数:"
echo "  if=   输入文件（/dev/zero 是无限输出 0 的设备）"
echo "  of=   输出文件"
echo "  bs=   每次读写块大小"
echo "  count= 块数（bs*count = 总大小）"
echo

dd if=/dev/zero of=userdata.img bs=4K count=1K status=none

echo "生成结果:"
echo "  大小(人类可读): $(du -h userdata.img | awk '{print $1}')"
echo "  字节数:         $(stat -c %s userdata.img)"
echo "  文件类型:       $(file userdata.img | sed 's/, UUID.*//')"

# cd / || exit 1
# rm -rf "$workdir"