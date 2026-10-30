#!/bin/bash
set -euxo pipefail
cd "$HOME/kernel-sw-prefetch"
exec > >(tee nop/runtime/commands.log) 2>&1
git rev-parse HEAD
git diff --exit-code -- include/uapi/linux/bpf.h kernel/bpf/verifier.c kernel/bpf/core.c kernel/bpf/disasm.c arch/x86/net/bpf_jit_comp.c
cp nop/test nop/runtime/test
cp nop/check nop/runtime/check
sha256sum build-jit/.config build-jit/arch/x86/boot/bzImage build/guest.cpio.gz nop/runtime/test nop/runtime/check > nop/runtime/SHA256SUMS
cat nop/runtime/SHA256SUMS
./nop/runtime/boot.sh auto > nop/runtime/guest.log 2>&1
grep -E 'GUEST_KERNEL|CONFIG_BPF_JIT|JIT |Program loaded|Return value|STATUS=|Checks:|FAIL:|loaded, retval' nop/runtime/guest.log
sha256sum -c nop/runtime/SHA256SUMS
