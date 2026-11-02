#!/bin/bash
set -e
set -o pipefail

cd /users/cs25m046
mkdir guest-original
cp /bin/busybox guest-original/busybox
cp /tmp/nop-test guest-original/test

cat > guest-original/init <<'INIT'
#!/bin/sh
/bin/busybox mount -t devtmpfs devtmpfs /dev
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys
/bin/busybox --install -s /bin
echo "Guest kernel: $(uname -r)"
echo 'Run: uname -r'
echo 'Then: /test'
echo 'Then: echo $?'
echo 'To shut down: poweroff -f'
exec /bin/sh </dev/console >/dev/console 2>&1
INIT

cat > guest-original/files <<'FILES'
dir /bin 0755 0 0
dir /dev 0755 0 0
dir /proc 0755 0 0
dir /sys 0755 0 0
dir /tmp 1777 0 0
nod /dev/console 0600 0 0 c 5 1
nod /dev/null 0666 0 0 c 1 3
file /bin/busybox /users/cs25m046/guest-original/busybox 0755 0 0
slink /bin/sh busybox 0777 0 0
file /init /users/cs25m046/guest-original/init 0755 0 0
file /test /users/cs25m046/guest-original/test 0755 0 0
FILES

kernel-original/build/usr/gen_init_cpio guest-original/files | gzip -9 > guest-original/initramfs.cpio.gz
printf 'Guest filesystem prepared. QEMU has not been started.\n'
sha256sum guest-original/init guest-original/test guest-original/initramfs.cpio.gz
printf 'Archive contents:\n'
gzip -dc guest-original/initramfs.cpio.gz | cpio -it
