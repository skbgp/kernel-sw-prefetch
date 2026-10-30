#define _GNU_SOURCE
#include <linux/bpf.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#define NOP_CODE 0xe7

int main(void)
{
    struct bpf_insn program[] = {
        { .code = NOP_CODE },
        { .code = BPF_ALU64 | BPF_MOV | BPF_K,
          .dst_reg = BPF_REG_0, .imm = 7 },
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
        puts(log);
        return 1;
    }
    puts("Program loaded");

    /* Run on a buffer; no packet is sent. */
    unsigned char data[64] = {0};
    attr = (union bpf_attr){0};
    attr.test.prog_fd = fd;
    attr.test.data_in = (uintptr_t)data;
    attr.test.data_size_in = sizeof(data);
    attr.test.repeat = 1;
    if (syscall(__NR_bpf, BPF_PROG_TEST_RUN, &attr, sizeof(attr)) < 0) {
        perror("Run failed");
        close(fd);
        return 1;
    }
    printf("Return value: %u\n", attr.test.retval);
    int result = attr.test.retval == 7 ? 0 : 1;
    close(fd);
    return result;
}
