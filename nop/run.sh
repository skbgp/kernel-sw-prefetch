#!/bin/sh
set -u
/shared/test
printf 'NOP_TEST_STATUS=%s\n' "$?"
/shared/check "$(cat /shared/expected)"
printf 'CHECK_STATUS=%s\n' "$?"
insmod /shared/test_bpf.ko
printf 'REGRESSION_STATUS=%s\n' "$?"
dmesg
rmmod test_bpf
printf 'REGRESSION_UNLOAD_STATUS=%s\n' "$?"
