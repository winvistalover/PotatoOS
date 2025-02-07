#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/potatoos.kernel isodir/mango.elf
cat > isodir/boot/grub/grub.cfg << EOF
set color_normal=white/black
set menu_color_normal=white/black
set menu_color_highlight=white/blue
set timeout=5

menuentry "-> PotatoOS" --id pos {
    multiboot /mango.elf
}
EOF


grub-mkrescue -o potatoOS.iso isodir
