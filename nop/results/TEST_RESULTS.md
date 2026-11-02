# NOP test results

Original kernel: `6.15.0-g680b721a3232`.
Modified kernel: `6.15.0-gc9d2947cd680`.
Both configurations are byte-identical. `CONFIG_BPF_JIT=n`, `CONFIG_BPF_SYSCALL=y` and `CONFIG_XDP_BATCHING=y`.
QEMU uses one virtual CPU, 2048 MiB RAM and no network device.

The table below is checked against the saved raw console logs. A loader exit status of 1 is expected for a rejected program. It does not mean that the negative test failed. The console recorder itself exits successfully after normal guest shutdown; its exit code is separate from the test's exit status.

| Kernel | Test | Observed result | Test exit status | Expected result observed | Raw log |
|---|---|---|---:|---|---|
| Original | NOP before MOV | `unknown opcode e7` | 1 | Yes | [original-guest.log](original-guest.log) |
| Modified | NOP before MOV | `Program returned 7` | 0 | Yes | [modified-guest.log](modified-guest.log) |
| Modified | NOP imm=1 | `BPF_NOP uses reserved fields` | 1 | Yes | [invalid-immediate-guest.log](invalid-immediate-guest.log) |
| Modified | NOP dst_reg=1 | `BPF_NOP uses reserved fields` | 1 | Yes | [invalid-dst-guest.log](invalid-dst-guest.log) |
| Modified | NOP src_reg=1 | `BPF_NOP uses reserved fields` | 1 | Yes | [invalid-src-guest.log](invalid-src-guest.log) |
| Modified | NOP off=1 | `BPF_NOP uses reserved fields` | 1 | Yes | [invalid-off-guest.log](invalid-off-guest.log) |
| Modified | Preserve R0 across NOP | `Program returned 7` | 0 | Yes | [preserve-r0-guest.log](preserve-r0-guest.log) |
| Modified | NOP then EXIT without setting R0 | `R0 !read_ok` | 1 | Yes | [uninitialized-r0-guest.log](uninitialized-r0-guest.log) |
| Modified | Preserve R2 across NOP | `Program returned 7` | 0 | Yes | [preserve-r2-guest.log](preserve-r2-guest.log) |
| Modified | Preserve stack slot across NOP | `Program returned 7` | 0 | Yes | [preserve-stack-guest.log](preserve-stack-guest.log) |
| Modified | Two consecutive NOPs | `Program returned 7` | 0 | Yes | [two-nops-guest.log](two-nops-guest.log) |
| Modified | Opcode E4 | `unknown opcode e4` | 1 | Yes | [invalid-class-guest.log](invalid-class-guest.log) |
| Modified | Opcode EF | `unknown opcode ef` | 1 | Yes | [invalid-source-form-guest.log](invalid-source-form-guest.log) |
| Original | MOV and EXIT without NOP | `Program returned 7` | 0 | Yes | [control-original-guest.log](control-original-guest.log) |
| Modified | MOV and EXIT without NOP | `Program returned 7` | 0 | Yes | [control-modified-guest.log](control-modified-guest.log) |
| Modified | NOP off=-1 | `BPF_NOP uses reserved fields` | 1 | Yes | [invalid-negative-off-guest.log](invalid-negative-off-guest.log) |
| Modified | NOP imm=-1 | `BPF_NOP uses reserved fields` | 1 | Yes | [invalid-negative-imm-guest.log](invalid-negative-imm-guest.log) |
| Modified | Read R2 after NOP without setting R2 | `R2 !read_ok` | 1 | Yes | [uninitialized-r2-guest.log](uninitialized-r2-guest.log) |
| Modified | MOV then NOP without EXIT | `last insn is not an exit or jmp` | 1 | Yes | [missing-exit-guest.log](missing-exit-guest.log) |
| Modified | Initial input-length branch test | `Length 64: returned 9, expected 7` | 1 | No; test expectation corrected | [input-jump-guest.log](input-jump-guest.log) |
| Modified | Corrected input-dependent branch through NOP | `64 bytes -> 7; 32 bytes -> 9` | 0 | Yes | [input-jump-fixed-guest.log](input-jump-fixed-guest.log) |

21 recorded runs: 8 passing loader invocations with successful execution checks, 12 expected load rejections and one failed branch-test attempt. The failed attempt and the corrected successful rerun are both recorded. The corrected branch-test invocation executes the same loaded program twice, once per buffer length. These are focused tests, not a full kernel regression suite or proof that every edge case is covered.

The E4 and EF cases fail opcode recognition. Nonzero operand fields in E7 reach the reserved-field rule. The R0 and R2 initialization tests reach existing register-read validation.

## Not established by these runs

- JIT support or JIT-on behavior for the new instruction.
- Results from the full BPF selftests or syzkaller. The test_bpf module was run separately; see REGRESSION_RESULTS.md.
- Performance, packet batching behavior or SIMD execution.
- Exhaustive combinations of reserved fields, boundary values or control-flow paths.

Further coverage can include register-field boundaries and inspection of translated bytecode for the input-dependent jump test, and further control-flow cases. Existing test_bpf regression suites have now passed on both kernels; see REGRESSION_RESULTS.md.

The full source and raw logs are included in this branch. The separate LaTeX report is not part of this upload.
