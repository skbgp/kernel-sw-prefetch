#!/bin/sh
exec sudo -n qemu-system-x86_64 \
    -accel kvm -cpu host -m 2048 -smp 1 \
    -nographic -monitor none -no-reboot -nic none \
    -kernel /users/cs25m046/kernel-sw-prefetch/build/arch/x86/boot/bzImage \
    -initrd /users/cs25m046/guest-original/initramfs-no-r0.cpio.gz \
    -append 'console=ttyS0 quiet loglevel=3 panic=1'
