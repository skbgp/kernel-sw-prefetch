# NOP instruction

This branch adds a NOP instruction to the Linux 6.15 software-prefetch research kernel. The instruction uses opcode `0xe7`. It leaves registers and memory unchanged and continues to the next instruction.

The starting commit is `680b721a3232f6d689a67c1ca37c6cf35fe1df6e`. The source revision used for testing is `c9d2947cd6804952cb55e8e9a2797efee2a71f77`.

## Code changes

- `include/uapi/linux/bpf.h`: define the operation as `BPF_NOP = 0xe0`.
- `kernel/bpf/verifier.c`: require the exact E7 encoding and zero register, offset and immediate fields; record that NOP reads and writes no registers.
- `kernel/bpf/core.c`: add NOP to the instruction map and continue to the next instruction in its interpreter handler.
- `kernel/bpf/disasm.c`: print `(e7) nop` in instruction logs.

The complete change is saved in [results/nop.patch](results/nop.patch). No JIT implementation is included. The tests used the interpreter with `CONFIG_BPF_JIT=n`.

## Tests and results

`test.c` loads NOP, moves 7 into R0 and exits. It prints the verifier log. If loading succeeds, it uses `BPF_PROG_TEST_RUN` on a local buffer and checks the result. The program is not attached to a socket or network interface.

Other C files change one field, check preserved state, check invalid reads, or test control flow. Each has its own preparation and runtime logs.

The [focused-test results](results/TEST_RESULTS.md) cover 21 loader invocations: eight passed their execution checks, twelve were rejected as expected, and the first input-length branch test failed its expected-value check. Both that failure and the corrected successful rerun are saved. The corrected branch test runs the same loaded program with two input sizes.

The [regression results](results/REGRESSION_RESULTS.md) compare the existing `test_bpf` module on both kernels. Each reported 1053 BPF tests, 10 tail-call tests and two skb-segmentation tests passing, with zero failures.

These tests do not cover every possible program. JIT execution, the full BPF selftest suite, Syzkaller and performance measurements were not run as part of these records.

## Files

- `*.c`: test sources.
- `boot-*.sh`: exact QEMU commands, using one virtual CPU, 2048 MiB RAM and no network device.
- `prepare-guest.sh`: the initial guest filesystem preparation script.
- `guest/`: startup script and saved guest file manifests.
- `configs/`: default, BPF-enabled, baseline and regression configurations.
- `results/*.log`: all raw configuration, edit, commit, build and runtime logs, including failures.
- `results/*readable.txt`: console copies with terminal control sequences and carriage returns removed.
- `results/*summary.txt`: labeled excerpts of the regression summaries.
- `SHA256SUMS`: hashes of the packaged files.

Raw logs are kept unchanged. `github-state.log` is a snapshot taken before publication, so it records that this branch had not yet been pushed. The commit logs retain the actual identities and dates used at the time.

## Build and boot

The commands below run on an x86-64 Linux machine with the usual kernel build tools, BusyBox, cpio and QEMU/KVM installed. The recorded host was running Ubuntu kernel `6.8.0-147-generic`; the actual tests ran inside the Linux 6.15 QEMU guests.

For the original copy:

```sh
git clone --single-branch --branch main https://github.com/bpf-endeavor/kernel-sw-prefetch.git kernel-original
cd kernel-original
make O=build x86_64_defconfig
scripts/config --file build/.config -e BPF_SYSCALL -e XDP_BATCHING
make O=build olddefconfig
make O=build -j24 bzImage
```

For a separate copy with NOP:

```sh
git clone --single-branch --branch nop https://github.com/skbgp/kernel-sw-prefetch.git kernel-sw-prefetch
cd kernel-sw-prefetch
mkdir -p build
cp ../kernel-original/build/.config build/.config
make O=build olddefconfig
make O=build -j24 bzImage
gcc -static -Wall -Wextra -O2 nop/test.c -o /tmp/nop-test
```

`XDP_BATCHING` was needed because this fork's VirtIO driver uses batching fields outside their configuration guards. The first failed build is preserved in `results/original-build.log`; the successful rebuild is in `results/original-build-batching.log`. NOP itself does not require packet batching.

The saved guest scripts and manifests use `/users/cs25m046` from the experiment. Adjust these paths when using another account. `prepare-guest.sh` creates `guest-original` and is intended for a new guest directory. The other manifests record the files added for each individual test. Compile the corresponding C file before packaging that test's manifest.

Inside a test guest, run the relevant executable and read its exit status immediately:

```sh
uname -r
/test
echo $?
poweroff -f
```

For the regression module, enable `TEST_BPF=m` in both copies and build each kernel's own module:

```sh
scripts/config --file build/.config -m TEST_BPF
make O=build olddefconfig
make O=build -j24 bzImage modules
```

The matching `build/lib/test_bpf.ko` is included as `/test_bpf.ko` in each regression guest. Its test command is `insmod /test_bpf.ko`; the results are read with `dmesg`. The module is then removed with `rmmod test_bpf`. Full console recordings contain the commands and exit statuses.

## Reading the failures

- Original E7, E4 and EF: rejected by opcode recognition.
- E7 with nonzero reserved fields: rejected with `BPF_NOP uses reserved fields`.
- Uninitialised R0 or R2: rejected with `R0 !read_ok` or `R2 !read_ok`.
- Program ending in NOP: rejected with `last insn is not an exit or jmp`.
- First branch-test attempt: the supplied 64-byte buffer was not the length seen by the socket filter. The test setup removes the 14-byte Ethernet header. `jump-nop-fixed.c` compares against `64 - ETH_HLEN`; both input sizes then return the expected values. See [the notes](results/input-jump-notes.txt).
- First liveness edit script: stopped before writing because its text anchor did not match. The retry log records the successful edit.

For a negative test, loader exit status 1 is the expected result. The console recorder's exit code after guest shutdown is separate from that test status.
