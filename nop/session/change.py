from pathlib import Path

p = Path('kernel/bpf/core.c')
s = p.read_text()
old = '\tINSN_3(ALU64, MOD,  K),\t\t\t\\\n'
assert s.count(old) == 1
s = s.replace(old, old + '\tINSN_3(ALU64, NOP,  K),\t\t\t\\\n')
old = '\tALU64_MOV_K:\n\t\tDST = IMM;\n\t\tCONT;\n'
assert s.count(old) == 1
s = s.replace(old, old + '\tALU64_NOP_K:\n\t\tCONT;\n')
p.write_text(s)

p = Path('kernel/bpf/verifier.c')
s = p.read_text()
old = '\tcase BPF_ALU:\n\tcase BPF_ALU64:\n\t\tswitch (code) {\n'
assert s.count(old) == 1
s = s.replace(old, old + '\t\tcase BPF_NOP:\n\t\t\tuse = 0;\n\t\t\tdef = 0;\n\t\t\tbreak;\n')
p.write_text(s)

p = Path('kernel/bpf/disasm.c')
s = p.read_text()
old = '\tif (class == BPF_ALU || class == BPF_ALU64) {\n\t\tif (BPF_OP(insn->code) == BPF_END) {'
new = '\tif (class == BPF_ALU || class == BPF_ALU64) {\n\t\tif (insn->code == (BPF_ALU64 | BPF_NOP | BPF_K)) {\n\t\t\tverbose(cbs->private_data, "(%02x) nop\\n", insn->code);\n\t\t} else if (BPF_OP(insn->code) == BPF_END) {'
assert s.count(old) == 1
p.write_text(s.replace(old,new))
