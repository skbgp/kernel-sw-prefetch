#!/bin/sh
set -u
/shared/test
printf 'NOP_TEST_STATUS=%s\n' "$?"
/shared/check modified
printf 'CHECK_STATUS=%s\n' "$?"
dmesg
