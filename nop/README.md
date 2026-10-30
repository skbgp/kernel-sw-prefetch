# NOP instruction

This work adds one experimental eBPF instruction to the verifier and software interpreter. It is a first step towards adding vector instructions.

NOP leaves eBPF registers and memory unchanged and moves to the next instruction. The complete opcode is `0xe7`. The destination, source, offset and immediate fields must all be zero.

The kernel is based on Linux 6.15. The original research commit is `680b721a3232f6d689a67c1ca37c6cf35fe1df6e`. It already contains the Beeswax prefetching and batching changes. The NOP patch changes four kernel files. It does not change the x86 JIT.

The source links below use commit `b04bb3ec8d732ce6ba22106b551d34a78189c6ef`. The test sources at that commit use 7 as their expected return value. Older logs use 42.

## Start with the test program

There are two programs to keep track of. `test.c` is an ordinary userspace C program. Its `program[]` array contains the eBPF instructions that it submits to Linux.

```c
#define NOP_CODE 0xe7

struct bpf_insn program[] = {
    { .code = NOP_CODE },
    { .code = BPF_ALU64 | BPF_MOV | BPF_K,
      .dst_reg = BPF_REG_0, .imm = 7 },
    { .code = BPF_JMP | BPF_EXIT },
};
```

NOP continues to the next instruction. MOV puts 7 in R0. EXIT returns R0. The number 7 gives the test an expected result to check. It is not part of NOP's meaning. A program containing only NOP and EXIT would leave R0 uninitialised and would be rejected.

The small test calls `BPF_PROG_LOAD` to load the program and `BPF_PROG_TEST_RUN` to execute it. Loading returns a program file descriptor. Execution writes the eBPF return value into `attr.test.retval`. The C test returns shell status 0 only when its checks pass.

Read [test.c](test.c), [check.c](check.c), and the [line-by-line explanation](tests-explained.md).

## Follow the loading request

The starting example was [Independent-bpf-execution](https://github.com/smartnic/Independent-bpf-execution). Its C program builds an instruction array and loads it through `bpf_load_program()`. Our small test uses a direct system call. The [Linux BPF manual](https://man7.org/linux/man-pages/man2/bpf.2.html) describes the load command and request fields.

In `test.c`, line 29 submits the instructions:

```c
int fd = syscall(__NR_bpf, BPF_PROG_LOAD, &attr, sizeof(attr));
```

`__NR_bpf` identifies the BPF system call. `BPF_PROG_LOAD` selects the load command. `&attr` passes the address of the request. `sizeof(attr)` passes its size. The request contains the instruction address, instruction count, program type, licence declaration and log settings.

We followed this request through the kernel:

| File and line | What happens |
|---|---|
| [syscall.c:5833](https://github.com/skbgp/kernel-sw-prefetch/blob/b04bb3ec8d732ce6ba22106b551d34a78189c6ef/kernel/bpf/syscall.c#L5833) | The command dispatcher calls `bpf_prog_load()`. |
| [syscall.c:2759](https://github.com/skbgp/kernel-sw-prefetch/blob/b04bb3ec8d732ce6ba22106b551d34a78189c6ef/kernel/bpf/syscall.c#L2759) | The loading function begins. |
| [syscall.c:2971](https://github.com/skbgp/kernel-sw-prefetch/blob/b04bb3ec8d732ce6ba22106b551d34a78189c6ef/kernel/bpf/syscall.c#L2971) | It calls `bpf_check()`. |
| [verifier.c:23930](https://github.com/skbgp/kernel-sw-prefetch/blob/b04bb3ec8d732ce6ba22106b551d34a78189c6ef/kernel/bpf/verifier.c#L23930) | The verifier's main function begins. |
| [verifier.c:24040](https://github.com/skbgp/kernel-sw-prefetch/blob/b04bb3ec8d732ce6ba22106b551d34a78189c6ef/kernel/bpf/verifier.c#L24040) | It calls `resolve_pseudo_ldimm64()`, which also checks opcode recognition. |
| [syscall.c:2975](https://github.com/skbgp/kernel-sw-prefetch/blob/b04bb3ec8d732ce6ba22106b551d34a78189c6ef/kernel/bpf/syscall.c#L2975) | After verification, loading selects the execution runtime. |

The verifier also checks control flow, computes register liveness and checks each instruction's effect on program state. Finding a function's definition does not tell us when it runs; we followed its callers to establish the order.

## Read the instruction format

The existing structure is at [bpf.h:83](https://github.com/skbgp/kernel-sw-prefetch/blob/b04bb3ec8d732ce6ba22106b551d34a78189c6ef/include/uapi/linux/bpf.h#L83):

```c
struct bpf_insn {
    __u8 code;
    __u8 dst_reg:4;
    __u8 src_reg:4;
    __s16 off;
    __s32 imm;
};
```

An ordinary instruction occupies eight bytes: one for the opcode, one shared by the register fields, two for the offset and four for the immediate. The 64-bit immediate-load instruction takes two slots. We did not change this structure.

For ALU instructions, the opcode is split into these fields:

```text
field:  operation | source | class
bits:    7 6 5 4  |   3    | 2 1 0
E7:      1 1 1 0  |   0    | 1 1 1
```

The existing definitions in `include/uapi/linux/bpf_common.h` extract them:

```c
#define BPF_CLASS(code) ((code) & 0x07)
#define BPF_OP(code)    ((code) & 0xf0)
#define BPF_SRC(code)   ((code) & 0x08)
```

For E7, these return `07`, `e0` and `00`. The operation stays in the upper bits because the macro masks the value without shifting it. We read `bpf_common.h` to understand the format; the NOP patch does not change it.

## Choose and test the encoding

The original kernel defines `BPF_END = 0xd0`. Its `check_alu_op()` rejects operation fields above that value at original `verifier.c:15509`. This suggested investigating E0. We then checked the definitions, recognised-instruction map, interpreter handlers and x86 JIT cases for the complete E7 byte.

```text
BPF_ALU64 | BPF_NOP | BPF_K
   0x07   |  0xe0   | 0x00   = 0xe7
```

`0xe0` is the operation field. `0xe7` is the complete opcode. Bitwise OR combines their bits, regardless of the order of the macro names. `BPF_K` selects the immediate-source form; it does not itself require an immediate of zero. Our NOP rule requires that separately.

E0 also appears as `BPF_JCOND` in the jump class. With class 05 it gives E5, while our ALU64 encoding gives E7. We chose ALU64 for this experiment. That choice does not decide the eventual vector instruction's class or encoding.

E7 is a local extension to this fork. Its availability must be checked again when changing kernel versions.

The original kernel rejected the submitted instruction with:

```text
Load failed: Invalid argument
unknown opcode e7
```

Searching for that message led to the early recognition check:

```c
if (!bpf_opcode_in_insntable(insn->code)) {
    verbose(env, "unknown opcode %02x\n", insn->code);
    return -EINVAL;
}
```

It is at original `verifier.c:20222`, or [modified verifier.c:20231](https://github.com/skbgp/kernel-sw-prefetch/blob/b04bb3ec8d732ce6ba22106b551d34a78189c6ef/kernel/bpf/verifier.c#L20231). It asks whether the complete opcode is recognised. This runs before the main ALU checks, so an ALU verifier case alone would not make E7 load.

## Kernel changes

The full patch is [nop.patch](nop.patch). It adds 22 lines and removes one across four files. The six change locations are listed below. Line numbers refer to the modified source snapshot linked above.

### Define the operation

At `include/uapi/linux/bpf.h:30-32`, we added:

```c
/* NOP */
#define BPF_NOP 0xe0
```

This follows the existing operation definitions and gives E0 a name. It does not add execution support by itself.

### Register the opcode

At `kernel/bpf/core.c:1610`, inside `BPF_INSN_MAP`, we added:

```c
INSN_3(ALU64, NOP, K), \
```

The map begins at line 1550. Its use at line 1702 generates the recognised-opcode table, and its use at line 1734 generates the interpreter jump table. The entry therefore makes E7 recognised and associates it with `ALU64_NOP_K`.

`INSN_3` takes three macro arguments. It does not mean three instructions. The `##` operator joins tokens to form names. The interpreter's `&&ALU64_NOP_K` is the GNU C syntax for a label address. The trailing backslash continues the surrounding macro.

### Check the fields

At `kernel/bpf/verifier.c:15331-15339`, inside `check_alu_op()`, we added:

```c
if (opcode == BPF_NOP) {
    if (insn->code != (BPF_ALU64 | BPF_NOP | BPF_K) ||
        insn->dst_reg || insn->src_reg || insn->off || insn->imm) {
        verbose(env, "BPF_NOP uses reserved fields\n");
        return -EINVAL;
    }
    return 0;
}
```

The function starts at line 15325 and extracts the operation at line 15328. The main dispatcher sends ALU and ALU64 instructions to it at line 19515.

The outer condition identifies our operation. The inner condition requires the complete E7 encoding and zero in each unused field. Any nonzero field makes the logical OR condition true and causes rejection. We chose zero because this NOP has no operands. A zero destination field does not mean that NOP writes to R0.

`verbose()` writes the diagnostic. `-EINVAL` reports an invalid instruction. Returning zero means this check passed; the rest of the program must still verify. Alternative encodings E4 and EF are already excluded by the earlier opcode table, so the full-byte comparison is also a safeguard.

### Describe register use

At `kernel/bpf/verifier.c:23781-23784`, inside `compute_insn_live_regs()`, we added:

```c
case BPF_NOP:
    use = 0;
    def = 0;
    break;
```

These bitmasks describe which registers an instruction reads and writes. Zero means the set is empty. These assignments do not clear any eBPF register. NOP needs its own case because normal ALU instructions can read or write a destination register.

### Execute NOP

At `kernel/bpf/core.c:1847-1848`, we added:

```c
ALU64_NOP_K:
    CONT;
```

This label is inside `___bpf_prog_run()`. The existing macro at line 1751 is:

```c
#define CONT ({ insn++; goto select_insn; })
```

It advances the instruction pointer and returns to dispatch. It leaves eBPF register values and memory unchanged. The interpreter dispatches through `goto *jumptable[insn->code]` at line 1755.

### Print NOP in logs

At `kernel/bpf/disasm.c:194-196`, we added a NOP case before the existing END case:

```c
if (insn->code == (BPF_ALU64 | BPF_NOP | BPF_K)) {
    verbose(cbs->private_data, "(%02x) nop\n", insn->code);
} else if (BPF_OP(insn->code) == BPF_END) {
    /* Existing END handling continues here. */
```

This prints `(e7) nop`. It makes the logs readable. It does not decide acceptance or execution.

## Tests and results

`test.c` checks the small program. `check.c` runs twenty cases: the control, six valid NOP programs and thirteen programs that should reject. It checks values in R0, R2 and a stack slot, repeated NOPs, a submitted jump layout, uninitialised registers, missing EXIT, reserved fields and unsupported encodings.

In modified mode, seven programs should load and return the marker, and thirteen should reject. In original mode, only the control should load. The mode changes the checker's expectations; it does not change the kernel. Both modes can therefore report 20/20 while accepting different programs.

The accepted interpreter cases require a successful information query, `jited_prog_len=0`, successful execution and the expected return value. Zero JIT length alone does not prove correct execution. Rejected cases require EINVAL or EACCES and a nonempty log; the checker does not yet match the exact diagnostic text.

The first modified checker run passed 19/20. An unconditional jump left an unreachable MOV, which the verifier rejected with `unreachable insn 2`. The test was changed to use a conditional jump; the NOP kernel code did not change. That condition is constant, so the verifier can simplify it. The revised test checks acceptance and the final result but does not establish runtime conditional-branch coverage.

The earlier original-versus-modified comparison used the same userspace tests and configuration. The regression module was built separately for each kernel.

| Recorded check | Original kernel | Modified kernel |
|---|---|---|
| E7 load | Rejected: `unknown opcode e7` | Accepted |
| Small NOP test | Not executed | Returned 42 |
| Control | Returned 42 | Returned 42 |
| Focused checker | 20/20 expected outcomes | 20/20 expected outcomes |
| BPF regression module | 1053 passed, 0 failed | 1053 passed, 0 failed |
| Tail-call tests | 10 passed, 0 failed | 10 passed, 0 failed |
| skb segmentation tests | 2 passed, 0 failed | 2 passed, 0 failed |

The original guest reports `6.15.0-g680b721a3232`. The first modified build reports `6.15.0-ga30cd906722a-dirty`; its saved patch records the uncommitted edits. The current source snapshot and these earlier build revisions differ. The test logs have not been changed to match newer commit names or return markers.

The later tests recompiled the userspace programs with marker 7 and reused the existing modified images:

| Configuration | NOP result | Checker |
|---|---|---|
| JIT not compiled in | Returned 7 | 20/20 |
| JIT compiled in, switched off before loading | Returned 7 | 20/20 |
| Same JIT-capable image, switch on | Returned 0 | 14/20 |

The 14/20 result belongs to `jit/check.c`, whose control is expected to have JIT code. All six valid NOP execution cases failed. The regular interpreter checker has different expectations for the control.

The unchanged x86 JIT cannot handle E7. In this tested fork, a JIT request can leave `__bpf_prog_ret0_warn` selected after compilation fails. The program then loads but warns and returns zero. With `CONFIG_BPF_JIT_ALWAYS_ON` off, switching JIT off before loading selects the real interpreter. See [the JIT test](jit/README.md) and [the runtime-switch test](runtime/README.md).

These are focused correctness tests. The full BPF selftest suite, a syzkaller fuzzing campaign and SIMD performance measurements were not run for this milestone.

## Build configuration

There is no tracked root `.config`. The smaller build started from `x86_64_defconfig`, which has the BPF syscall and JIT off. Enabling only `BPF_SYSCALL` exposed a build error in the unchanged VirtIO network driver. It references batching fields that are only defined with `CONFIG_XDP_BATCHING`.

Enabling `BPF_SYSCALL` and `XDP_BATCHING` produced the tested smaller image. Kconfig also resolved their dependencies. NOP itself does not need packet batching. See [the smaller-build notes](minimal/README.md).

The regression configuration also enables the test module, module loading, configuration display, serial console, initramfs and VirtIO 9P support. Its script explicitly requests JIT off. Later tests kept JIT compiled in and changed its runtime switch. Prefetch JIT support was off in these tests.

QEMU uses one virtual CPU and 2 GiB RAM. It has no network adapter. The tests run on a local buffer. VirtIO 9P shares the host's `nop/` directory as `/shared` in the guest.

## Commands and records

Follow [COMMANDS.md](COMMANDS.md) for a fresh build and manual demonstration. The [session scripts](session/) and [logs](results/) preserve the historical commands and outcomes. The [value-7 notes](results/return-7.md) explain the later rerun. Old logs and hashes retain their original values.

The next instruction needs a defined operation, operands and verifier rules. A TTL decrement is one candidate. The NOP work does not yet implement a vector operation or provide a SIMD speedup.
