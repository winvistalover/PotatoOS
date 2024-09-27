#!/bin/sh
set -e
. ./iso.sh
qemu-img create -f raw pos.img 100M
mformat -i pos.img :: -F
echo test >> test.txt
mcopy -i pos.img test.txt ::
qemu-system-$(./target-triplet-to-arch.sh $HOST) -cdrom potatoOS.iso -hda pos.img -boot d -net nic,model=virtio
