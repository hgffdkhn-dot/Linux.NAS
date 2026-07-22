#!/bin/bash
# 将源码写入临时文件，并用 base64 编码嵌入最终的 install.sh
cat > install.sh << 'EOF'
#!/bin/bash
set -e
echo "[1/4] 检查编译环境..."
if ! command -v gcc &> /dev/null || ! [ -d "/lib/modules/$(uname -r)/build" ]; then
    echo "错误: 缺少 gcc 或内核头文件 (linux-headers-$(uname -r))"
    exit 1
fi

TMP_DIR=$(mktemp -d)
cd $TMP_DIR

echo "[2/4] 编译内核模块..."
cat > kern_chan.c << 'EOC'
/* 此处粘贴上面 kern_chan.c 的全部源码 */
EOC

cat > Makefile << 'EOM'
obj-m += kern_chan.o
all:
    make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
    make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
EOM

make

echo "[3/4] 加载模块并创建设备通道..."
sudo insmod kern_chan.ko
# 等待设备生成
sleep 1

echo "[4/4] 完成！"
echo "========================================"
echo ">>> 内核接口连接地址: /dev/kern_chan"
echo ">>> 其他工具作者请在脚本中连接此路径"
echo ">>> (重启后模块自动失效，无需手动清理)"
echo "========================================"

# 清理编译文件，仅保留运行中的ko（重启即丢）
rm -f Makefile kern_chan.c *.mod *.o *.order *.symvers
EOF

chmod +x install.sh
echo "已生成 install.sh"