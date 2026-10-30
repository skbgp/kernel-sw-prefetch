#!/bin/sh
set -u
printf 'JIT before: '
cat /proc/sys/net/core/bpf_jit_enable
echo 0 > /proc/sys/net/core/bpf_jit_enable || exit 1
printf 'JIT during test: '
cat /proc/sys/net/core/bpf_jit_enable
/shared/test
printf 'NOP_TEST_STATUS=%s\n' "$?"
/shared/check modified
printf 'CHECK_STATUS=%s\n' "$?"
echo 1 > /proc/sys/net/core/bpf_jit_enable
printf 'RESTORE_STATUS=%s\n' "$?"
printf 'JIT after: '
cat /proc/sys/net/core/bpf_jit_enable
dmesg
