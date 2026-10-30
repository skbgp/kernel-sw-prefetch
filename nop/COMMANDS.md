# Build and test commands

Run the build and QEMU commands on an x86-64 Linux host with KVM available. They are not Mac KVM commands. QEMU boots a guest kernel without replacing the host kernel.

The commands below reproduce the setup with the current value-7 test sources. The saved historical original-versus-modified logs use 42. The exact old commands are in [session/](session/).

## Install the tools

```sh
sudo apt-get update
sudo apt-get install -y build-essential flex bison bc libssl-dev libelf-dev cpio qemu-system-x86 busybox-static git python3
```

GCC builds the userspace tests. Make builds the kernel. BusyBox and cpio provide the small guest filesystem. QEMU runs the guest.

## Get both source versions

Start in a new directory:

```sh
mkdir nop-demo
cd nop-demo
git clone --branch nop-verifier https://github.com/skbgp/kernel-sw-prefetch.git
cd kernel-sw-prefetch
git switch --detach b04bb3ec8d732ce6ba22106b551d34a78189c6ef
git worktree add --detach ../kernel-original 680b721a3232f6d689a67c1ca37c6cf35fe1df6e
cp -a nop ../kernel-original/nop
```

The fixed commits keep the source versions reproducible. The modified source already contains the NOP patch. Do not apply it again. Copying the helper folder into the original tree supplies the same tests without changing its kernel source. A worktree provides a separate checkout while sharing Git's stored objects. Create a development branch if you later want to edit the detached source.

## Inspect the changes

From `kernel-sw-prefetch`:

```sh
git diff 680b721a3232 -- include/uapi/linux/bpf.h
git diff 680b721a3232 -- kernel/bpf/verifier.c
git diff 680b721a3232 -- kernel/bpf/core.c
git diff 680b721a3232 -- kernel/bpf/disasm.c
git diff 680b721a3232 -- arch/x86/net/bpf_jit_comp.c
```

The last command should print nothing. The kernel patch changes four files at six places. The full patch is [nop.patch](nop.patch).

## Build the original

```sh
cd ../kernel-original
sh nop/configure.sh
gcc -static -Wall -Wextra -O2 nop/test.c -o nop/test
gcc -static -Wall -Wextra -O2 nop/check.c -o nop/check
printf 'original\n' > nop/expected
sh nop/guest.sh
make O=build -j24 bzImage modules
cp build/lib/test_bpf.ko nop/test_bpf.ko
./nop/boot.sh
```

`configure.sh` creates the regression-test configuration. `-static` makes the userspace executables suitable for the small guest. `-Wall -Wextra` enables warnings and `-O2` enables optimisation. `O=build` keeps generated files in `build/`. `-j24` allows 24 build jobs; reduce it on a smaller host. It does not set the number of guest CPUs.

`guest.sh` packs BusyBox, startup code and mount points into `build/guest.cpio.gz`. The kernel image is `build/arch/x86/boot/bzImage`. `modules` also builds `test_bpf.ko`, which must match the kernel being booted.

Inside the original guest:

```sh
uname -r
/shared/test
echo $?
/shared/check original
echo $?
poweroff -f
```

The small test should reject E7 and exit with status 1. The original-mode checker should count the expected outcomes and exit with status 0. Check the guest kernel version before interpreting the result.

## Build the modified kernel

After QEMU shuts down, these commands run on the host:

```sh
cd ../kernel-sw-prefetch
mkdir -p build
cp ../kernel-original/build/.config build/.config
make O=build olddefconfig
gcc -static -Wall -Wextra -O2 nop/test.c -o nop/test
gcc -static -Wall -Wextra -O2 nop/check.c -o nop/check
printf 'modified\n' > nop/expected
sh nop/guest.sh
make O=build -j24 bzImage modules
cp build/lib/test_bpf.ko nop/test_bpf.ko
./nop/boot.sh
```

Both trees use the same configuration and test sources. The regression module is built separately for each kernel.

Inside the modified guest:

```sh
uname -r
/shared/test
echo $?
/shared/check modified
echo $?
poweroff -f
```

The small test should print `Program loaded` and `Return value: 7`, then exit with status 0. The checker should print `Checks: 20/20 passed` and exit with status 0. This is the expected outcome when repeating the setup; the old logs remain the record of the earlier runs.

## Run the regression module

Inside either correctly prepared guest:

```sh
/shared/run.sh
```

This repeats the small test and checker, inserts the matching `test_bpf.ko`, prints the kernel log and unloads the module. Read `NOP_TEST_STATUS`, `CHECK_STATUS`, the regression statuses and the module summaries. The original small NOP test is expected to report status 1. A normal QEMU shutdown alone does not establish a test pass.

To save an automatic guest run on the host:

```sh
mkdir -p nop/results
set -o pipefail
./nop/boot.sh auto 2>&1 | tee nop/results/guest.log
```

The `auto` argument adds a guest command-line option that runs `/shared/run.sh` and powers off. `tee` both displays and saves the console output. `2>&1` includes error output.

## Use the interpreter in the JIT-capable image

After the separate JIT image has been built, boot it with:

```sh
./nop/runtime/boot.sh
```

Inside the guest, with `CONFIG_BPF_JIT_ALWAYS_ON` off:

```sh
cat /proc/sys/net/core/bpf_jit_enable
echo 0 > /proc/sys/net/core/bpf_jit_enable
/shared/test
/shared/check modified
echo 1 > /proc/sys/net/core/bpf_jit_enable
poweroff -f
```

The switch must be off before the test loads its program. It is a guest-wide setting for new loads. It does not convert an already loaded native program into an interpreted one. The recorded test restored an initial value of 1 after closing the programs. Restore the value you found if using another setup. See [runtime/README.md](runtime/README.md) and [jit/README.md](jit/README.md).

## Find the relevant source

These searches run from the kernel source directory:

```sh
rg -n -A 7 '^struct bpf_insn ' include/uapi/linux/bpf.h
rg -n 'BPF_CLASS|BPF_OP|BPF_SRC' include/uapi/linux/bpf_common.h
rg -n -A 4 'case BPF_PROG_LOAD:' kernel/bpf/syscall.c
rg -n 'bpf_check\(&prog' kernel/bpf/syscall.c
rg -n '^int bpf_check\(' kernel/bpf/verifier.c
rg -n 'unknown opcode|opcode > BPF_END|check_alu_op\(env, insn\)' kernel/bpf/verifier.c
rg -n 'BPF_NOP' include/uapi/linux/bpf.h kernel/bpf/verifier.c kernel/bpf/core.c kernel/bpf/disasm.c
rg -n 'INSN_3\(ALU64, NOP|ALU64_NOP_K|define CONT' kernel/bpf/core.c
```

If ripgrep is not installed, use `grep -n` for a single pattern or `grep -nE` for alternatives. Read the surrounding function after finding a match. The README gives the relevant line numbers at the fixed source revision.

## Read the saved results

```sh
gzip -dc nop/results/original-guest.log.gz | less
gzip -dc nop/results/modified-guest.log.gz | less
gzip -dc nop/results/return-7-minimal.log.gz | less
gzip -dc nop/results/return-7-runtime.log.gz | less
gzip -dc nop/results/return-7-jit.log.gz | less
```

The original and modified comparison logs use 42. The three later logs use 7. The original kernel was not rerun with 7 for those later records.

Record source and build identities when repeating a test:

```sh
git rev-parse HEAD
git status --short
sha256sum build/.config build/arch/x86/boot/bzImage nop/test nop/check nop/test_bpf.ko
```

The commit, uncommitted changes, configuration and binary hashes describe different parts of the experiment. The historical scripts in `session/` depend on their original starting state; read them rather than rerunning source-edit or publishing scripts on the final tree.
