#define _GNU_SOURCE
#include <linux/bpf.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

/* 0xe0 operation | 0x07 class | 0x00 source. */
#define NOP_CODE 0xe7

int main(void)
{
    /* Run NOP, put 7 in R0, then exit. */
    struct bpf_insn program[] = {
        { .code = NOP_CODE },
        { .code = BPF_ALU64 | BPF_MOV | BPF_K,
          .dst_reg = BPF_REG_0, .imm = 7 },
        { .code = BPF_JMP | BPF_EXIT },
    };
    /* The kernel writes verifier messages here. */
    char log[65536] = {0};
    union bpf_attr attr = {0};

    /* Tell the kernel where the bytecode and log buffer are. */
    attr.prog_type = BPF_PROG_TYPE_SOCKET_FILTER;
    attr.insn_cnt = sizeof(program) / sizeof(program[0]);
    attr.insns = (uintptr_t)program;
    attr.license = (uintptr_t)"GPL";
    attr.log_buf = (uintptr_t)log;
    attr.log_size = sizeof(log);
    attr.log_level = 1;

    /* Load the program and print the verifier log if loading fails. */
    int fd = syscall(__NR_bpf, BPF_PROG_LOAD, &attr, sizeof(attr));
    if (fd < 0) {
        perror("Load failed");
        printf("%s", log);
        return 1;
    }
    printf("Program loaded\n%s", log);

    /* Run once with a 64-byte test buffer. */
    char data[64] = {0};
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
    printf("Program returned %u\n", attr.test.retval);
    close(fd);
    /* Pass only if the eBPF program returns 7. */
    return attr.test.retval == 7 ? 0 : 1;
}
