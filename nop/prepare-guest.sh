#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 kernel-build-directory new-guest-directory" >&2
    exit 1
fi

build=$(CDPATH= cd -- "$1" && pwd)
src=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
guest=$(realpath -m -- "$2")
case "$guest" in
    *[[:space:]]*) echo "Use a guest directory path without spaces." >&2; exit 1 ;;
esac
mkdir "$guest"

cc -static -Wall -Wextra -O2 "$src/test.c" -o "$guest/test"
cc -static -Wall -Wextra -O2 "$src/check.c" -o "$guest/check"
cp /bin/busybox "$guest/busybox"

cat > "$guest/init" <<'INIT'
#!/bin/sh
/bin/busybox mount -t devtmpfs devtmpfs /dev
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys
/bin/busybox --install -s /bin
echo "Guest kernel: $(uname -r)"
echo 'Run /test, then /check. Run poweroff -f to shut down.'
exec /bin/sh </dev/console >/dev/console 2>&1
INIT

cat > "$guest/files" <<FILES
dir /bin 0755 0 0
dir /dev 0755 0 0
dir /proc 0755 0 0
dir /sys 0755 0 0
dir /tmp 1777 0 0
nod /dev/console 0600 0 0 c 5 1
nod /dev/null 0666 0 0 c 1 3
file /bin/busybox $guest/busybox 0755 0 0
slink /bin/sh busybox 0777 0 0
file /init $guest/init 0755 0 0
file /test $guest/test 0755 0 0
file /check $guest/check 0755 0 0
FILES

"$build/usr/gen_init_cpio" "$guest/files" > "$guest/initramfs.cpio"
gzip -9 "$guest/initramfs.cpio"
printf 'Guest ready: %s/initramfs.cpio.gz\n' "$guest"
