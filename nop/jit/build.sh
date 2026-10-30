#!/bin/bash
set -euxo pipefail
cd "$HOME/kernel-sw-prefetch"
exec > >(tee nop/jit/build.log) 2>&1
git rev-parse HEAD
git diff --exit-code -- include/uapi/linux/bpf.h kernel/bpf/verifier.c kernel/bpf/core.c kernel/bpf/disasm.c arch/x86/net/bpf_jit_comp.c
printf '/build-jit/\n' >> .git/info/exclude
mkdir -p build-jit
cp build/.config build-jit/.config
scripts/config --file build-jit/.config -e BPF_JIT -d BPF_JIT_ALWAYS_ON -d BPF_JIT_PREFETCH
make O=build-jit olddefconfig
diff -u build/.config build-jit/.config || test "$?" -eq 1
gcc -static -Wall -Wextra -O2 nop/jit/check.c -o nop/jit/check
cp nop/test nop/jit/test
time make O=build-jit -j24 bzImage modules
cp build-jit/lib/test_bpf.ko nop/jit/test_bpf.ko
sha256sum build-jit/.config build-jit/arch/x86/boot/bzImage nop/jit/check nop/jit/test nop/jit/test_bpf.ko
./nop/jit/boot.sh > nop/jit/guest.log 2>&1
grep -E 'GUEST_KERNEL|JIT |CONFIG_BPF_JIT|loaded, retval|NOP_TEST_STATUS|CHECK_STATUS|REGRESSION_.*STATUS|Summary:|Checks:|FAIL:|unknown opcode' nop/jit/guest.log
