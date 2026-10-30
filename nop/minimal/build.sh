#!/bin/bash
set -euxo pipefail
cd "$HOME/kernel-sw-prefetch"
exec > >(tee nop/minimal/build.log) 2>&1
git rev-parse HEAD
git diff --exit-code -- include/uapi/linux/bpf.h kernel/bpf/verifier.c kernel/bpf/core.c kernel/bpf/disasm.c arch/x86/net/bpf_jit_comp.c
printf '/build-minimal/\n' >> .git/info/exclude
make O=build-minimal x86_64_defconfig
cp build-minimal/.config nop/minimal/default.config
scripts/config --file build-minimal/.config -e BPF_SYSCALL
make O=build-minimal olddefconfig
cp build-minimal/.config nop/minimal/config
diff -u nop/minimal/default.config nop/minimal/config > nop/minimal/config.diff || test "$?" -eq 1
cat nop/minimal/config.diff
set +e
time make O=build-minimal -j24 bzImage
status=$?
set -e
printf 'BUILD_STATUS=%s\n' "$status"
printf '%s\n' "$status" > nop/minimal/build-status
exit "$status"
