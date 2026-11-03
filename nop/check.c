#define _GNU_SOURCE
#include <errno.h>
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define NOP_CODE 0xe7

struct test {
    const char *name;
    struct bpf_insn program[6];
    unsigned int count;
    const char *error;
    unsigned int runs;
    unsigned int expected[2];
};

static const struct test tests[] = {
    {
        .name = "class",
        .program = {
            { .code = 0xe4 },
            { .code = BPF_ALU64 | BPF_MOV | BPF_K,
              .dst_reg = BPF_REG_0, .imm = 7 },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 3,
        .error = "unknown opcode e4",
    },
    {
        .name = "source form",
        .program = {
            { .code = 0xef },
            { .code = BPF_ALU64 | BPF_MOV | BPF_K,
              .dst_reg = BPF_REG_0, .imm = 7 },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 3,
        .error = "unknown opcode ef",
    },
    {
        .name = "destination",
        .program = {
            { .code = NOP_CODE, .dst_reg = 1 },
            { .code = BPF_ALU64 | BPF_MOV | BPF_K,
              .dst_reg = BPF_REG_0, .imm = 7 },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 3,
        .error = "BPF_NOP uses reserved fields",
    },
    {
        .name = "source register",
        .program = {
            { .code = NOP_CODE, .src_reg = 1 },
            { .code = BPF_ALU64 | BPF_MOV | BPF_K,
              .dst_reg = BPF_REG_0, .imm = 7 },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 3,
        .error = "BPF_NOP uses reserved fields",
    },
    {
        .name = "positive offset",
        .program = {
            { .code = NOP_CODE, .off = 1 },
            { .code = BPF_ALU64 | BPF_MOV | BPF_K,
              .dst_reg = BPF_REG_0, .imm = 7 },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 3,
        .error = "BPF_NOP uses reserved fields",
    },
    {
        .name = "negative offset",
        .program = {
            { .code = NOP_CODE, .off = -1 },
            { .code = BPF_ALU64 | BPF_MOV | BPF_K,
              .dst_reg = BPF_REG_0, .imm = 7 },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 3,
        .error = "BPF_NOP uses reserved fields",
    },
    {
        .name = "positive immediate",
        .program = {
            { .code = NOP_CODE, .imm = 1 },
            { .code = BPF_ALU64 | BPF_MOV | BPF_K,
              .dst_reg = BPF_REG_0, .imm = 7 },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 3,
        .error = "BPF_NOP uses reserved fields",
    },
    {
        .name = "negative immediate",
        .program = {
            { .code = NOP_CODE, .imm = -1 },
            { .code = BPF_ALU64 | BPF_MOV | BPF_K,
              .dst_reg = BPF_REG_0, .imm = 7 },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 3,
        .error = "BPF_NOP uses reserved fields",
    },
    {
        .name = "uninitialised R0",
        .program = {
            { .code = NOP_CODE },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 2,
        .error = "R0 !read_ok",
    },
    {
        .name = "uninitialised R2",
        .program = {
            { .code = NOP_CODE },
            { .code = BPF_ALU64 | BPF_MOV | BPF_X,
              .dst_reg = BPF_REG_0, .src_reg = BPF_REG_2 },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 3,
        .error = "R2 !read_ok",
    },
    {
        .name = "missing exit",
        .program = {
            { .code = BPF_ALU64 | BPF_MOV | BPF_K,
              .dst_reg = BPF_REG_0, .imm = 7 },
            { .code = NOP_CODE },
        },
        .count = 2,
        .error = "last insn is not an exit or jmp",
    },
    {
        .name = "control",
        .program = {
            { .code = BPF_ALU64 | BPF_MOV | BPF_K,
              .dst_reg = BPF_REG_0, .imm = 7 },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 2,
        .runs = 1,
        .expected = {7},
    },
    {
        .name = "preserve R0",
        .program = {
            { .code = BPF_ALU64 | BPF_MOV | BPF_K,
              .dst_reg = BPF_REG_0, .imm = 7 },
            { .code = NOP_CODE },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 3,
        .runs = 1,
        .expected = {7},
    },
    {
        .name = "preserve R2",
        .program = {
            { .code = BPF_ALU64 | BPF_MOV | BPF_K,
              .dst_reg = BPF_REG_2, .imm = 7 },
            { .code = NOP_CODE },
            { .code = BPF_ALU64 | BPF_MOV | BPF_X,
              .dst_reg = BPF_REG_0, .src_reg = BPF_REG_2 },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 4,
        .runs = 1,
        .expected = {7},
    },
    {
        .name = "preserve stack",
        .program = {
            { .code = BPF_ST | BPF_MEM | BPF_DW,
              .dst_reg = BPF_REG_10, .off = -8, .imm = 7 },
            { .code = NOP_CODE },
            { .code = BPF_LDX | BPF_MEM | BPF_DW,
              .dst_reg = BPF_REG_0, .src_reg = BPF_REG_10, .off = -8 },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 4,
        .runs = 1,
        .expected = {7},
    },
    {
        .name = "two NOPs",
        .program = {
            { .code = BPF_ALU64 | BPF_MOV | BPF_K,
              .dst_reg = BPF_REG_0, .imm = 7 },
            { .code = NOP_CODE },
            { .code = NOP_CODE },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 4,
        .runs = 1,
        .expected = {7},
    },
    {
        .name = "input length",
        .program = {
            { .code = BPF_LDX | BPF_MEM | BPF_W,
              .dst_reg = BPF_REG_2, .src_reg = BPF_REG_1,
              .off = offsetof(struct __sk_buff, len) },
            { .code = BPF_ALU64 | BPF_MOV | BPF_K,
              .dst_reg = BPF_REG_0, .imm = 7 },
            { .code = BPF_JMP | BPF_JEQ | BPF_K,
              .dst_reg = BPF_REG_2, .off = 1, .imm = 64 - ETH_HLEN },
            { .code = BPF_ALU64 | BPF_MOV | BPF_K,
              .dst_reg = BPF_REG_0, .imm = 9 },
            { .code = NOP_CODE },
            { .code = BPF_JMP | BPF_EXIT },
        },
        .count = 6,
        .runs = 2,
        .expected = {7, 9},
    },
};

int main(void)
{
    char log[65536];
    char data[64] = {0};
    unsigned int lengths[] = {64, 32};
    unsigned int failed = 0;
    unsigned int count = sizeof(tests) / sizeof(tests[0]);

    for (unsigned int i = 0; i < count; i++) {
        const struct test *test = &tests[i];
        union bpf_attr attr = {0};
        memset(log, 0, sizeof(log));
        attr.prog_type = BPF_PROG_TYPE_SOCKET_FILTER;
        attr.insn_cnt = test->count;
        attr.insns = (uintptr_t)test->program;
        attr.license = (uintptr_t)"GPL";
        attr.log_buf = (uintptr_t)log;
        attr.log_size = sizeof(log);
        attr.log_level = 1;

        int fd = syscall(__NR_bpf, BPF_PROG_LOAD, &attr, sizeof(attr));
        if (fd < 0) {
            int error = errno;
            if (test->error && strstr(log, test->error)) {
                printf("PASS %s: rejected as expected\n%s", test->name, log);
            } else {
                printf("FAIL %s: %s\n%s", test->name, strerror(error), log);
                failed++;
            }
            continue;
        }
        if (test->error) {
            printf("FAIL %s: program was accepted\n%s", test->name, log);
            failed++;
            close(fd);
            continue;
        }
        printf("Loaded %s\n%s", test->name, log);

        for (unsigned int run = 0; run < test->runs; run++) {
            attr = (union bpf_attr){0};
            attr.test.prog_fd = fd;
            attr.test.data_in = (uintptr_t)data;
            attr.test.data_size_in = lengths[run];
            attr.test.repeat = 1;
            if (syscall(__NR_bpf, BPF_PROG_TEST_RUN, &attr, sizeof(attr)) < 0) {
                printf("FAIL %s: %s\n", test->name, strerror(errno));
                failed++;
                continue;
            }
            if (attr.test.retval != test->expected[run]) {
                printf("FAIL %s: length %u returned %u, expected %u\n",
                       test->name, lengths[run], attr.test.retval,
                       test->expected[run]);
                failed++;
            } else {
                printf("PASS %s: length %u returned %u\n",
                       test->name, lengths[run], attr.test.retval);
            }
        }
        close(fd);
    }
    printf("%u cases, %u failures\n", count, failed);
    return failed ? 1 : 0;
}
