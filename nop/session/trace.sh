#!/bin/bash
set -euxo pipefail
cd "$HOME/kernel-sw-prefetch"
git log -2 --oneline
git status --short --branch
git diff --check
git diff 680b721a3 --stat
git diff 680b721a3 -- arch/x86/net/bpf_jit_comp.c
grep -nE '#define BPF_(NOP|ALU64|END|JCOND)' include/uapi/linux/bpf.h
grep -n -A 7 '^struct bpf_insn {' include/uapi/linux/bpf.h
grep -nE 'BPF_CLASS|BPF_OP|BPF_SRC' include/uapi/linux/bpf_common.h
grep -nE 'static int bpf_prog_load|bpf_check\(&prog|bpf_prog_select_runtime|case BPF_PROG_LOAD|case BPF_PROG_TEST_RUN|SYSCALL_DEFINE3\(bpf' kernel/bpf/syscall.c
grep -nE 'static int check_alu_op|static int do_check\(|unknown opcode|bpf_opcode_in_insntable|compute_insn_live_regs|int bpf_check\(' kernel/bpf/verifier.c
grep -n -B 3 -A 4 'opcode > BPF_END' kernel/bpf/verifier.c
grep -n -B 3 -A 12 'opcode == BPF_NOP' kernel/bpf/verifier.c
grep -n -B 3 -A 8 'case BPF_NOP' kernel/bpf/verifier.c
grep -nE 'INSN_3\(ALU64, NOP|ALU64_NOP_K|define CONT|BPF_INSN_3_TBL|BPF_INSN_3_LBL|bpf_prog_select_runtime|bpf_prog_select_func' kernel/bpf/core.c
grep -n -B 4 -A 5 '"(%02x) nop' kernel/bpf/disasm.c
grep -n -A 12 'config TEST_BPF' lib/Kconfig.debug
cat tools/testing/selftests/net/test_bpf.sh
git diff 680b721a3 -- include/uapi/linux/bpf.h kernel/bpf/verifier.c kernel/bpf/core.c kernel/bpf/disasm.c > "$HOME/nop-records/nop.patch"
scripts/checkpatch.pl --no-tree "$HOME/nop-records/nop.patch"
