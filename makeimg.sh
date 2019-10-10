#!/bin/bash

dd if=/dev/zero of=ext2_test.img bs=516096c count=200

losetup -o0 /dev/loop100 ext2_test.img
mke2fs -b1024 /dev/loop100

mkdir -p mnt
mount -text2 /dev/loop100 mnt

cp -r sysroot/* mnt

cd mnt

cp ../initrd/programs/kilo.c kilo.c

sync

cd ..

umount /dev/loop100
losetup -d /dev/loop100

chmod 777 ext2_test.img