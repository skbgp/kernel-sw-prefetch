# NOP

This adds opcode `0xe7` to the Linux 6.15 software-prefetch kernel. NOP leaves registers and memory alone and moves to the next instruction. The interpreter runs it with `CONFIG_BPF_JIT=n`.

The change is in `include/uapi/linux/bpf.h`, `kernel/bpf/verifier.c`, `kernel/bpf/core.c` and `kernel/bpf/disasm.c`.

## Run

Use x86-64 Linux with static BusyBox, cpio and QEMU/KVM. From the kernel directory:

```sh
make O=build x86_64_defconfig
scripts/config --file build/.config -e BPF_SYSCALL -e XDP_BATCHING
make O=build olddefconfig
make O=build -j24 bzImage
nop/prepare-guest.sh build /tmp/nop-guest
sudo nop/boot.sh build/arch/x86/boot/bzImage /tmp/nop-guest/initramfs.cpio.gz
```

In the guest, run `/test`, then `/check`. The first prints `Program returned 7`. The second checks the invalid encodings, register and stack state, and a branch. It exits with 0 only when every check passes. Run `poweroff -f` to shut down.

`XDP_BATCHING` is needed to build the existing VirtIO driver in this fork.

The [test results](results/TEST_RESULTS.md) and [regression results](results/REGRESSION_RESULTS.md) describe the saved runs. The [original test files and boot scripts](https://github.com/skbgp/kernel-sw-prefetch/tree/8304da5f0a3b947da236a454c3ad78554fb7ea17/nop) are the versions used for those logs.
