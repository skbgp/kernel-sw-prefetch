#!/bin/sh
set -eu
make O=build x86_64_defconfig
scripts/config --file build/.config \
    -e BPF_SYSCALL -d BPF_JIT -d BPF_JIT_ALWAYS_ON -d BPF_JIT_PREFETCH \
    -e MODULES -e MODULE_UNLOAD -m TEST_BPF \
    -e IKCONFIG -e IKCONFIG_PROC \
    -e NET_9P -e NET_9P_VIRTIO -e 9P_FS -e VIRTIO_PCI \
    -e DEVTMPFS -e DEVTMPFS_MOUNT -e BLK_DEV_INITRD \
    -e SERIAL_8250 -e SERIAL_8250_CONSOLE \
    -e XDP_BATCHING -d DEBUG_INFO -d DEBUG_INFO_BTF -e DEBUG_INFO_NONE \
    --set-str SYSTEM_TRUSTED_KEYS '' --set-str SYSTEM_REVOCATION_KEYS ''
make O=build olddefconfig
