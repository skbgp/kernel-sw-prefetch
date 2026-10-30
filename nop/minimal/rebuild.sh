#!/bin/bash
set -euxo pipefail
cd "$HOME/kernel-sw-prefetch"
exec > >(tee nop/minimal/rebuild.log) 2>&1
cp nop/minimal/config nop/minimal/bpf-only.config
cp nop/minimal/config.diff nop/minimal/bpf-only.diff
scripts/config --file build-minimal/.config -e XDP_BATCHING
make O=build-minimal olddefconfig
cp build-minimal/.config nop/minimal/config
diff -u nop/minimal/default.config nop/minimal/config > nop/minimal/config.diff || test "$?" -eq 1
cat nop/minimal/config.diff
time make O=build-minimal -j24 bzImage
sha256sum build-minimal/arch/x86/boot/bzImage
