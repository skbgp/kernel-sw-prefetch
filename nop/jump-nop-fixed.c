#define _GNU_SOURCE
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#define NOP_CODE 0xe7

int main(void)
{
    struct bpf_insn program[] = {
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
    };
    char log[65536] = {0};
    union bpf_attr attr = {0};

    attr.prog_type = BPF_PROG_TYPE_SOCKET_FILTER;
    attr.insn_cnt = sizeof(program) / sizeof(program[0]);
    attr.insns = (uintptr_t)program;
    attr.license = (uintptr_t)"GPL";
    attr.log_buf = (uintptr_t)log;
    attr.log_size = sizeof(log);
    attr.log_level = 1;

    int fd = syscall(__NR_bpf, BPF_PROG_LOAD, &attr, sizeof(attr));
    if (fd < 0) {
        perror("Load failed");
        printf("%s", log);
        return 1;
    }
    printf("Program loaded\n%s", log);

    char data[64] = {0};
    unsigned int lengths[] = {64, 32};
    unsigned int expected[] = {7, 9};

    for (unsigned int i = 0; i < 2; i++) {
        attr = (union bpf_attr){0};
        attr.test.prog_fd = fd;
        attr.test.data_in = (uintptr_t)data;
        attr.test.data_size_in = lengths[i];
        attr.test.repeat = 1;

        if (syscall(__NR_bpf, BPF_PROG_TEST_RUN, &attr, sizeof(attr)) < 0) {
            perror("Run failed");
            close(fd);
            return 1;
        }
        printf("Length %u: returned %u, expected %u\n",
               lengths[i], attr.test.retval, expected[i]);
        if (attr.test.retval != expected[i]) {
            close(fd);
            return 1;
        }
    }
    close(fd);
    return 0;
}
