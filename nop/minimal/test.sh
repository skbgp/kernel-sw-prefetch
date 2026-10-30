#!/bin/bash
set -euxo pipefail
cd "$HOME/kernel-sw-prefetch"
exec > >(tee nop/minimal/test.log) 2>&1
cp nop/test nop/minimal/test
cp nop/check nop/minimal/check
mkdir -p build-minimal/guest
cp -a build/guest/. build-minimal/guest/
cp nop/minimal/init build-minimal/guest/init
chmod +x build-minimal/guest/init
(cd build-minimal/guest && find . -print0 | cpio --null -o --format=newc) | gzip -9 > build-minimal/guest.cpio.gz
sha256sum build-minimal/.config build-minimal/arch/x86/boot/bzImage build-minimal/guest.cpio.gz nop/minimal/test nop/minimal/check > nop/minimal/SHA256SUMS
./nop/minimal/boot.sh auto > nop/minimal/guest.log 2>&1
grep -E 'GUEST_KERNEL|Program loaded|Return value|STATUS=|Checks:|FAIL:|loaded, retval' nop/minimal/guest.log
sha256sum -c nop/minimal/SHA256SUMS
