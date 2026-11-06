# NOP

This adds opcode `0xe7` to the Linux 6.15 software-prefetch kernel. NOP leaves registers and memory alone and moves to the next instruction. The interpreter runs it with `CONFIG_BPF_JIT=n`.

The kernel change is in `include/uapi/linux/bpf.h`, `kernel/bpf/verifier.c`, `kernel/bpf/core.c` and `kernel/bpf/disasm.c`. The same NOP definition is also in `tools/include/uapi/linux/bpf.h` for bpftool.

## Run

Use x86-64 Linux with static BusyBox and QEMU/KVM. From the kernel directory:

```sh
make O=build x86_64_defconfig
scripts/config --file build/.config -e BPF_SYSCALL -e XDP_BATCHING
make O=build olddefconfig
make O=build -j24 bzImage
nop/prepare-guest.sh build /tmp/nop-guest
sudo nop/boot.sh build/arch/x86/boot/bzImage /tmp/nop-guest/initramfs.cpio.gz
```

In the modified guest, run `/test`, then `/check`. The first prints `Program returned 7`. The second checks the invalid encodings, register and stack state, and a branch. It exits with 0 only when every check passes. Run `poweroff -f` to shut down.

`XDP_BATCHING` is needed to build the existing VirtIO driver in this fork.

## Timing

Run `/bench modified` in the modified guest, or `/bench original` in the original guest. It prints the verifier time, total loading time and repeated execution time for the control, MOV and NOP programs. The original mode skips NOP because that kernel does not support it. Results and raw logs are kept with the report.
