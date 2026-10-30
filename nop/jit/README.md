# JIT test

E7 passes verification but does not execute correctly with JIT enabled in the tested kernel. The x86 JIT has no E7 handler. The NOP patch did not change that backend.

The image reports `6.15.0-g4f71e5812b52`. Its configuration includes:

```text
CONFIG_BPF_JIT=y
# CONFIG_BPF_JIT_ALWAYS_ON is not set
CONFIG_BPF_JIT_DEFAULT_ON=y
```

`BPF_JIT_PREFETCH` is off. The guest runtime switch was 1. The interpreter was still compiled in because ALWAYS_ON was off, but that alone did not give a working fallback.

## Recorded results

| Check | Earlier run with 42 | Later run with 7 |
|---|---|---|
| Control program | Returned 42, JIT length 31 | Returned 7, JIT length 31 |
| Small NOP program | Loaded, returned 0 | Loaded, returned 0 |
| Six valid NOP cases | All returned 0 | All returned 0 |
| Checker | 14/20 | 14/20 |
| Small test exit status | 1 | 1 |

The invalid cases were rejected as expected. The six valid NOP execution cases failed. This directory's `check.c` expects JIT code for the control. The regular `../check.c` expects no JIT code for accepted programs, so the recorded 14/20 must not be attributed to that regular checker.

## Why it returned zero

Program allocation records whether JIT was requested. With the switch on, `bpf_prog_select_func()` installs `__bpf_prog_ret0_warn` as a placeholder, expecting JIT compilation to replace it.

The unchanged x86 backend encounters E7, logs `bpf_jit: unknown opcode e7` and fails compilation. In this tested configuration, loading still succeeds and keeps the placeholder. `BPF_PROG_TEST_RUN` then reaches that function, which warns and returns zero.

A zero `jited_prog_len` therefore does not prove that the real interpreter ran. Both the runtime path and the returned value need checking. Switching JIT off before loading selected the interpreter in the [separate runtime test](../runtime/README.md).

## Build and run

These commands assume the earlier regression build and guest filesystem exist:

```sh
mkdir -p build-jit
cp build/.config build-jit/.config
scripts/config --file build-jit/.config -e BPF_JIT -d BPF_JIT_ALWAYS_ON -d BPF_JIT_PREFETCH
make O=build-jit olddefconfig
gcc -static -Wall -Wextra -O2 nop/jit/check.c -o nop/jit/check
cp nop/test nop/jit/test
make O=build-jit -j24 bzImage modules
cp build-jit/lib/test_bpf.ko nop/jit/test_bpf.ko
./nop/jit/boot.sh
```

`boot.sh` without arguments opens an interactive guest. For the recorded automatic sequence, use:

```sh
./nop/jit/boot.sh auto
```

The full build script is [build.sh](build.sh). The guest has one CPU, 2 GiB RAM, a read-only share and no network adapter. Read the test statuses; a normal shutdown does not mean the NOP passed.

## Incomplete regression run

An earlier JIT-enabled guest also tried to insert `test_bpf.ko`. It stopped showing progress and was terminated before a module summary appeared. Its cause was not established. It is not counted as a regression pass or attributed to the NOP patch.

Records: [source trace](source-trace.log.gz), [build](build.log.gz), [guest](guest.log.gz), [incomplete module run](regression-attempt.log.gz), [configuration](config), [hashes](SHA256SUMS), and [later value-7 run](../results/return-7.md).
