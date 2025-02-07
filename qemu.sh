#!/bin/sh
set -e
. ./iso.sh
qemu-img create -f raw pos.img 100M
mformat -i pos.img :: -F
echo test >> test.txt
mcopy -i pos.img test.txt ::
qemu-system-$(./target-triplet-to-arch.sh $HOST) -device isa-debug-exit,iobase=0xf4,iosize=0x04  -m 256M -cdrom potatoOS.iso -hda pos.img -boot d -net nic,model=virtio -serial mon:stdio -smp 1 -usb -cpu pentium2 -vga cirrus