# Smaller build

This test started from the research kernel's `x86_64_defconfig` and kept the existing NOP source patch. The guest had one CPU, 2 GiB RAM and no network adapter.

The default has the BPF syscall and JIT off. The first attempt enabled only the syscall:

```sh
make O=build-minimal x86_64_defconfig
scripts/config --file build-minimal/.config -e BPF_SYSCALL
make O=build-minimal olddefconfig
make O=build-minimal -j24 bzImage
```

It failed in `drivers/net/virtio_net.c` with:

```text
error: 'struct receive_queue' has no member named 'xdp_rx_batch'
```

The driver references batching fields in allocation and cleanup code, but their definitions depend on `CONFIG_XDP_BATCHING`. The driver is unchanged by the NOP patch. This build problem was already present in the research fork.

The next attempt enabled batching and rebuilt:

```sh
scripts/config --file build-minimal/.config -e XDP_BATCHING
make O=build-minimal olddefconfig
make O=build-minimal -j24 bzImage
```

It succeeded. The two explicit additions to the default were:

```text
CONFIG_BPF_SYSCALL=y
CONFIG_XDP_BATCHING=y
```

Kconfig also resolved dependencies. The full difference is in [config.diff](config.diff). JIT stayed off, as it was in the default; this experiment did not explicitly disable it. NOP does not need packet batching. Enabling batching supplied definitions needed by this driver build.

This image does not enable the regression module or `/proc/config.gz`. The saved build configuration is [config](config). Showing that saved file is different from reading a configuration from the running kernel.

## Recorded result

The source revision was `dffd96610d566db842011011c3b443cc959916ce`. The guest reported:

```text
GUEST_KERNEL=6.15.0-gdffd96610d56
Program loaded
Return value: 42
NOP_TEST_STATUS=0
Checks: 20/20 passed
CHECK_STATUS=0
```

The later test recompiled the userspace programs with 7 and reused this image. It returned 7 and passed 20/20. See [the later results](../results/return-7.md). The regression module was not run in this smaller configuration.

## Repeat the experiment

The historical scripts assume the earlier test binaries and `build/guest` BusyBox filesystem exist. The [main command guide](../COMMANDS.md) explains how to create them.

```sh
bash nop/minimal/build.sh
bash nop/minimal/rebuild.sh
bash nop/minimal/test.sh
```

Run these separately. `build.sh` records the expected failed attempt. `rebuild.sh` saves that configuration, enables batching and rebuilds. `test.sh` prepares this guest and runs its tests.

After the image and guest files exist, an interactive run is:

```sh
./nop/minimal/boot.sh
```

Inside QEMU:

```sh
uname -r
/shared/test
echo $?
/shared/check modified
echo $?
poweroff -f
```

The test sources now expect 7. The old logs still show 42. The shared files must contain executables compiled from the intended source version.

Records: [failed build](build.log.gz), [rebuild](rebuild.log.gz), [test commands](test.log.gz), [guest log](guest.log.gz), and [hashes](SHA256SUMS).
