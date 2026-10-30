#!/bin/sh
set -u
printf 'JIT before: '
cat /proc/sys/net/core/bpf_jit_enable
echo 1 > /proc/sys/net/core/bpf_jit_enable
printf 'JIT during test: '
cat /proc/sys/net/core/bpf_jit_enable
/shared/test
printf 'NOP_TEST_STATUS=%s\n' "$?"
/shared/check
printf 'CHECK_STATUS=%s\n' "$?"
insmod /shared/test_bpf.ko
printf 'REGRESSION_STATUS=%s\n' "$?"
dmesg
rmmod test_bpf
printf 'REGRESSION_UNLOAD_STATUS=%s\n' "$?"
