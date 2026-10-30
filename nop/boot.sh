#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
extra=
if [ "${1:-}" = auto ]; then
    extra=nop.auto=1
fi
exec sudo qemu-system-x86_64 \
    -accel kvm -cpu host -m 2048 -smp 1 \
    -nographic -no-reboot -monitor none -nic none \
    -kernel build/arch/x86/boot/bzImage \
    -initrd build/guest.cpio.gz \
    -fsdev local,id=share,path="$PWD/nop",security_model=none,readonly=on \
    -device virtio-9p-pci,fsdev=share,mount_tag=tests \
    -append "console=ttyS0 quiet loglevel=3 panic=1 $extra"
