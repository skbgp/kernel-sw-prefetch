#!/bin/bash
set -euxo pipefail
cd "$HOME/kernel-sw-prefetch"
exec > >(tee nop/results/return-7-commands.log) 2>&1
git rev-parse HEAD
gcc -static -Wall -Wextra -O2 nop/test.c -o nop/test
gcc -static -Wall -Wextra -O2 nop/check.c -o nop/check
gcc -static -Wall -Wextra -O2 nop/jit/check.c -o nop/jit/check
cp nop/test nop/minimal/test
cp nop/check nop/minimal/check
cp nop/test nop/runtime/test
cp nop/check nop/runtime/check
cp nop/test nop/jit/test
./nop/minimal/boot.sh auto > nop/results/return-7-minimal.log 2>&1
./nop/runtime/boot.sh auto > nop/results/return-7-runtime.log 2>&1
./nop/jit/boot.sh > nop/results/return-7-jit.log 2>&1
for name in minimal runtime jit; do
    grep -E 'GUEST_KERNEL|JIT before|JIT during|JIT after|Return value|STATUS=|Checks:|control: loaded' "nop/results/return-7-$name.log"
done
sha256sum nop/test nop/check nop/jit/check build-minimal/arch/x86/boot/bzImage build-jit/arch/x86/boot/bzImage > nop/results/return-7-sha256.txt
grep -q 'Return value: 7' nop/results/return-7-minimal.log
grep -q 'Checks: 20/20 passed' nop/results/return-7-minimal.log
grep -q 'Return value: 7' nop/results/return-7-runtime.log
grep -q 'Checks: 20/20 passed' nop/results/return-7-runtime.log
grep -q 'Return value: 0' nop/results/return-7-jit.log
grep -q 'Checks: 14/20 passed' nop/results/return-7-jit.log
sha256sum -c nop/results/return-7-sha256.txt
