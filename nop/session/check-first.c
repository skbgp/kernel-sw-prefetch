#define _GNU_SOURCE
#include <errno.h>
#include <linux/bpf.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define NOP { .code = 0xe7 }
#define MOV(r, v) { .code = BPF_ALU64 | BPF_MOV | BPF_K, .dst_reg = r, .imm = v }
#define EXIT { .code = BPF_JMP | BPF_EXIT }

struct test {
    const char *name;
    int count;
    int accept;
    struct bpf_insn insns[8];
};

int main(int argc, char **argv)
{
    if (argc != 2 || (strcmp(argv[1], "original") && strcmp(argv[1], "modified"))) {
        fprintf(stderr, "Usage: %s original|modified\n", argv[0]);
        return 1;
    }
    int nop = !strcmp(argv[1], "modified");
    struct test tests[] = {
        { "control", 2, 1, { MOV(0, 42), EXIT } },
        { "NOP first", 3, nop, { NOP, MOV(0, 42), EXIT } },
        { "NOP preserves r0", 3, nop, { MOV(0, 42), NOP, EXIT } },
        { "two NOPs", 4, nop, { MOV(0, 42), NOP, NOP, EXIT } },
        { "NOP preserves r2", 4, nop, { MOV(2, 42), NOP,
            { .code = BPF_ALU64 | BPF_MOV | BPF_X, .dst_reg = 0, .src_reg = 2 }, EXIT } },
        { "NOP preserves stack", 4, nop, {
            { .code = BPF_ST | BPF_MEM | BPF_DW, .dst_reg = 10, .off = -8, .imm = 42 }, NOP,
            { .code = BPF_LDX | BPF_MEM | BPF_DW, .dst_reg = 0, .src_reg = 10, .off = -8 }, EXIT } },
        { "jump to NOP", 5, nop, { MOV(0, 42),
            { .code = BPF_JMP | BPF_JA, .off = 1 }, MOV(0, 7), NOP, EXIT } },
        { "uninitialized return", 2, 0, { NOP, EXIT } },
        { "uninitialized read after NOP", 3, 0, { NOP,
            { .code = BPF_ALU64 | BPF_MOV | BPF_X, .dst_reg = 0, .src_reg = 2 }, EXIT } },
        { "missing exit", 2, 0, { MOV(0, 42), NOP } },
        { "dst 1", 3, 0, { { .code = 0xe7, .dst_reg = 1 }, MOV(0, 42), EXIT } },
        { "dst 15", 3, 0, { { .code = 0xe7, .dst_reg = 15 }, MOV(0, 42), EXIT } },
        { "src 1", 3, 0, { { .code = 0xe7, .src_reg = 1 }, MOV(0, 42), EXIT } },
        { "offset 1", 3, 0, { { .code = 0xe7, .off = 1 }, MOV(0, 42), EXIT } },
        { "offset -1", 3, 0, { { .code = 0xe7, .off = -1 }, MOV(0, 42), EXIT } },
        { "immediate 1", 3, 0, { { .code = 0xe7, .imm = 1 }, MOV(0, 42), EXIT } },
        { "immediate -1", 3, 0, { { .code = 0xe7, .imm = -1 }, MOV(0, 42), EXIT } },
        { "ALU32 encoding e4", 3, 0, { { .code = 0xe4 }, MOV(0, 42), EXIT } },
        { "register encoding ef", 3, 0, { { .code = 0xef }, MOV(0, 42), EXIT } },
        { "unsupported f7", 3, 0, { { .code = 0xf7 }, MOV(0, 42), EXIT } },
    };
    unsigned int passed = 0;
    for (unsigned int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        struct test *t = &tests[i];
        char log[65536] = {0};
        union bpf_attr attr = {0};
        attr.prog_type = BPF_PROG_TYPE_SOCKET_FILTER;
        attr.insn_cnt = t->count;
        attr.insns = (uintptr_t)t->insns;
        attr.license = (uintptr_t)"GPL";
        attr.log_buf = (uintptr_t)log;
        attr.log_size = sizeof(log);
        attr.log_level = 1;
        int fd = syscall(__NR_bpf, BPF_PROG_LOAD, &attr, sizeof(attr));
        int error = errno;
        int ok = 0;
        if (fd < 0) {
            ok = !t->accept && (error == EINVAL || error == EACCES) && log[0];
            printf("%s: load rejected (%s)\n%s", t->name, strerror(error), log);
        } else {
            struct bpf_prog_info info = {0};
            attr = (union bpf_attr){0};
            attr.info.bpf_fd = fd;
            attr.info.info_len = sizeof(info);
            attr.info.info = (uintptr_t)&info;
            int info_ok = syscall(__NR_bpf, BPF_OBJ_GET_INFO_BY_FD, &attr, sizeof(attr)) == 0;
            unsigned char data[64] = {0};
            attr = (union bpf_attr){0};
            attr.test.prog_fd = fd;
            attr.test.data_in = (uintptr_t)data;
            attr.test.data_size_in = sizeof(data);
            attr.test.repeat = 1;
            int run_ok = syscall(__NR_bpf, BPF_PROG_TEST_RUN, &attr, sizeof(attr)) == 0;
            ok = t->accept && info_ok && !info.jited_prog_len && run_ok && attr.test.retval == 42;
            printf("%s: loaded, retval=%u, jited_prog_len=%u\n", t->name, attr.test.retval, info.jited_prog_len);
            close(fd);
        }
        printf("%s: %s\n", ok ? "PASS" : "FAIL", t->name);
        passed += ok;
    }
    unsigned int total = sizeof(tests) / sizeof(tests[0]);
    printf("Checks: %u/%u passed\n", passed, total);
    return passed == total ? 0 : 1;
}
