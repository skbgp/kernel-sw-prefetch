# Select the interpreter

This test reused the JIT-capable image. It changed the guest's runtime switch before loading the NOP program. No kernel rebuild or JIT source change was needed.

The configuration has JIT compiled in and `CONFIG_BPF_JIT_ALWAYS_ON` off. The image reports `6.15.0-g4f71e5812b52`.

## Recorded result

```text
JIT before: 1
JIT during test: 0
Program loaded
Return value: 42
NOP_TEST_STATUS=0
Checks: 20/20 passed
CHECK_STATUS=0
RESTORE_STATUS=0
JIT after: 1
```

All accepted cases returned 42 with `jited_prog_len=0`. The later test used 7 with the same image and again passed 20/20. See [the later results](../results/return-7.md).

The test restored the runtime switch after closing the program descriptors. The host's setting was not changed. The regression module was not repeated in this experiment.

## Run manually

After the JIT image and guest files exist, run this on the Linux host:

```sh
./nop/runtime/boot.sh
```

Inside the guest:

```sh
uname -r
cat /proc/sys/net/core/bpf_jit_enable
echo 0 > /proc/sys/net/core/bpf_jit_enable
/shared/test
echo $?
/shared/check modified
echo $?
echo 1 > /proc/sys/net/core/bpf_jit_enable
poweroff -f
```

The recorded initial switch value was 1. Restore the initial value if using another setup. The current shared test binaries must be compiled from the intended test sources, which now use 7.

The switch must be off before loading. This is a guest-wide setting for new loads, not a per-program BPF load flag. Changing it does not convert an already loaded native program into an interpreted one.

With the switch off, allocation records `jit_requested` as false and `bpf_prog_select_func()` selects an interpreter. Execution reaches `ALU64_NOP_K`, follows `CONT`, moves the marker into R0 and exits. With the switch on, the [JIT test](../jit/README.md) reaches the warning placeholder instead.

ALWAYS_ON was not tested. That configuration removes the software interpreter and does not allow selecting it with this runtime switch.

## Files

The recorded host sequence is [start.sh](start.sh). It assumes the earlier images, test binaries and guest filesystem exist. It copies the tests, checks hashes and boots automatically.

```sh
bash nop/runtime/start.sh
```

Other records are [run.sh](run.sh), [boot.sh](boot.sh), [host log](commands.log.gz), [guest log](guest.log.gz), [configuration](../jit/config), and [hashes](SHA256SUMS). The guest uses one CPU and a local input buffer, with no network adapter.
