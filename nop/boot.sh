#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 kernel-image initramfs" >&2
    exit 1
fi

exec qemu-system-x86_64 \
    -accel kvm -cpu host -m 2048 -smp 1 \
    -nographic -monitor none -no-reboot -nic none \
    -kernel "$1" -initrd "$2" \
    -append 'console=ttyS0 quiet loglevel=3 panic=1'
