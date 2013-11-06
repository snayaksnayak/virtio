#!/bin/bash

#make clean
#make

cp ./bin/kernel.bin ./bin/kernel

losetup /dev/loop15 ./harddisk.img
mount /dev/loop15 /mnt/floppy
cp ./bin/kernel /mnt/floppy
cp ./bin/kernel.bin /mnt/floppy
umount /dev/loop15
losetup -d /dev/loop15

#for old qemu v0.91
#../qemu/bin/qemu -L ../qemu/share/ -no-kqemu -m 64 -hda ./harddisk.img

#for new compiled qemu
#qemu-system-x86_64 -m 64 -vga vmware -serial stdio -hda ./harddisk.img
#qemu-system-x86_64 -m 64 -hda ./harddisk.img


#for qemu with virtio nic enabled
#qemu-system-i386 -m 64 -vga vmware -serial stdio -hda ./harddisk.img -net nic,model=virtio

#for qemu with virtio nic and virtio disk enabled
qemu-system-x86_64 -m 64 -vga vmware -serial stdio -hda ./harddisk.img -net nic,model=virtio -drive file=harddisk2.img,if=virtio

#for qemu with virtio nic and two virtio disk enabled
#qemu-system-x86_64 -m 64 -vga vmware -serial stdio -hda ./harddisk.img -net nic,model=virtio -drive file=harddisk2.img,if=virtio -drive file=harddisk3.img,if=virtio
