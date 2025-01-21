#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/potatoos.kernel isodir/boot/potatoos.kernel
cat > isodir/boot/grub/grub.cfg << EOF
DEFAULT=pos
TIMEOUT=5
vga=0x31A
color_normal=white/black
color_highlight=white/blue


menuentry "-> PotatoOS"  --id pos{
	multiboot /boot/potatoos.kernel
}
EOF
grub-mkrescue -o potatoOS.iso isodir
