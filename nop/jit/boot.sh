#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
exec sudo qemu-system-x86_64 \
    -accel kvm -cpu host -m 2048 -smp 1 \
    -nographic -no-reboot -monitor none -nic none \
    -kernel build-jit/arch/x86/boot/bzImage \
    -initrd build/guest.cpio.gz \
    -fsdev local,id=share,path="$PWD/nop/jit",security_model=none,readonly=on \
    -device virtio-9p-pci,fsdev=share,mount_tag=tests \
    -append 'console=ttyS0 quiet loglevel=3 panic=1 nop.auto=1'
