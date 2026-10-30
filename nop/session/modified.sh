#!/bin/bash
set -euxo pipefail
exec > >(tee "$HOME/nop-records/modified-build.log") 2>&1
cd "$HOME/kernel-sw-prefetch"
python3 "$HOME/change-nop.py"
git diff --check
git diff 680b721a3 -- include/uapi/linux/bpf.h kernel/bpf/verifier.c kernel/bpf/core.c kernel/bpf/disasm.c
cp -a ../kernel-original/nop .
mkdir -p build
cp ../kernel-original/build/.config build/.config
make O=build olddefconfig
printf 'modified\n' > nop/expected
sh -x nop/guest.sh
time make O=build -j24 bzImage modules
cp build/lib/test_bpf.ko nop/test_bpf.ko
sha256sum build/.config build/arch/x86/boot/bzImage nop/test nop/check nop/test_bpf.ko
