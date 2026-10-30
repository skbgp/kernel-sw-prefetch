#!/bin/sh
set -eu
mkdir -p build/guest/bin build/guest/dev build/guest/proc build/guest/sys build/guest/shared
cp /bin/busybox build/guest/bin/busybox
for app in sh mount uname cat grep zcat poweroff setsid cttyhack insmod rmmod dmesg; do
    ln -sf busybox "build/guest/bin/$app"
done
cp nop/init build/guest/init
chmod +x build/guest/init
(cd build/guest && find . -print0 | cpio --null -o --format=newc) | gzip -9 > build/guest.cpio.gz
