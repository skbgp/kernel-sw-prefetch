#!/bin/bash
set -euxo pipefail
exec > >(tee "$HOME/nop-records/baseline-build.log") 2>&1
cd "$HOME/kernel-original"
printf '/build/\n' >> ../kernel-sw-prefetch/.git/info/exclude
sh -x nop/configure.sh
gcc -static -Wall -Wextra -O2 nop/test.c -o nop/test
gcc -static -Wall -Wextra -O2 nop/check.c -o nop/check
printf 'original\n' > nop/expected
sh -x nop/guest.sh
time make O=build -j24 bzImage modules
cp build/lib/test_bpf.ko nop/test_bpf.ko
sha256sum build/.config build/arch/x86/boot/bzImage nop/test nop/check nop/test_bpf.ko
