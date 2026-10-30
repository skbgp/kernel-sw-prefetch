#!/bin/bash
set -euxo pipefail
exec > >(tee "$HOME/nop-records/rerun.log") 2>&1
for tree in kernel-original kernel-sw-prefetch; do
    cd "$HOME/$tree"
    cp nop/results/guest.log nop/results/first-run.log
    cp "$HOME/check-nop.c" nop/check.c
    gcc -static -Wall -Wextra -O2 nop/check.c -o nop/check
    ./nop/boot.sh auto > nop/results/guest.log 2>&1
    grep -E 'GUEST_KERNEL|NOP_TEST_STATUS|Checks:|CHECK_STATUS|REGRESSION_.*STATUS|Summary:|FAIL:' nop/results/guest.log
    sha256sum nop/test nop/check build/.config build/arch/x86/boot/bzImage build/guest.cpio.gz nop/test_bpf.ko
    git status --short --branch
 done
