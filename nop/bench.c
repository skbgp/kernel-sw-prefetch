#define _GNU_SOURCE
#include <linux/bpf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#define NOP_CODE 0xe7
#define LOG_STATS 4
#define REPEAT 10000
#define RUN_SAMPLES 31

struct test {
    const char *name;
    unsigned int extra;
    struct bpf_insn program[1026];
    unsigned int count;
};

static struct test tests[11];
static unsigned int count;
static char logbuf[65536];

static void stop(const char *message)
{
    perror(message);
    exit(1);
}

static uint64_t now(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &t))
        stop("clock_gettime");
    return (uint64_t)t.tv_sec * 1000000000 + t.tv_nsec;
}

static void add(const char *name, unsigned int extra)
{
    struct test *t = &tests[count++];
    t->name = name;
    t->extra = extra;
    t->count = extra + 2;
    t->program[0] = (struct bpf_insn){
        .code = BPF_ALU64 | BPF_MOV | BPF_K, .imm = 7
    };
    for (unsigned int i = 1; i <= extra; i++) {
        if (!strcmp(name, "nop"))
            t->program[i] = (struct bpf_insn){ .code = NOP_CODE };
        else
            t->program[i] = t->program[0];
    }
    t->program[extra + 1] = (struct bpf_insn){ .code = BPF_JMP | BPF_EXIT };
}

/* Time only the load syscall. Verifier time comes from the kernel log. */
static int load(const struct test *t, uint64_t *elapsed,
                unsigned long long *verify, unsigned int *processed)
{
    union bpf_attr attr = {0};
    memset(logbuf, 0, sizeof(logbuf));
    attr.prog_type = BPF_PROG_TYPE_SOCKET_FILTER;
    attr.insn_cnt = t->count;
    attr.insns = (uintptr_t)t->program;
    attr.license = (uintptr_t)"GPL";
    attr.log_buf = (uintptr_t)logbuf;
    attr.log_size = sizeof(logbuf);
    attr.log_level = LOG_STATS;
    uint64_t start = now();
    int fd = syscall(__NR_bpf, BPF_PROG_LOAD, &attr, sizeof(attr));
    *elapsed = now() - start;
    if (fd < 0) {
        fprintf(stderr, "%s", logbuf);
        stop("BPF_PROG_LOAD");
    }
    char *v = strstr(logbuf, "verification time ");
    char *p = strstr(logbuf, "processed ");
    if (!v || !p || sscanf(v, "verification time %llu usec", verify) != 1 ||
        sscanf(p, "processed %u insns", processed) != 1) {
        fprintf(stderr, "Missing verifier statistics:\n%s", logbuf);
        exit(1);
    }
    return fd;
}

/* Check that NOPs survive translation and that the program is not JITed. */
static void inspect(int fd, const struct test *t)
{
    struct bpf_insn translated[1026] = {0};
    struct bpf_prog_info info = {0};
    union bpf_attr attr = {0};
    info.xlated_prog_insns = (uintptr_t)translated;
    info.xlated_prog_len = sizeof(translated);
    attr.info.bpf_fd = fd;
    attr.info.info_len = sizeof(info);
    attr.info.info = (uintptr_t)&info;
    if (syscall(__NR_bpf, BPF_OBJ_GET_INFO_BY_FD, &attr, sizeof(attr)))
        stop("BPF_OBJ_GET_INFO_BY_FD");
    unsigned int nops = 0;
    for (unsigned int i = 0; i < info.xlated_prog_len / sizeof(translated[0]); i++)
        nops += translated[i].code == NOP_CODE;
    printf("# info,%s,%u,submitted=%u,translated=%u,nops=%u,jit_bytes=%u,verified=%u\n",
           t->name, t->extra, t->count,
           info.xlated_prog_len / (unsigned int)sizeof(translated[0]),
           nops, info.jited_prog_len, info.verified_insns);
    if (info.jited_prog_len || info.xlated_prog_len != t->count * sizeof(translated[0]) ||
        nops != (!strcmp(t->name, "nop") ? t->extra : 0)) {
        fprintf(stderr, "Unexpected translated program or JIT state\n");
        exit(1);
    }
}

/* duration is the kernel's average nanoseconds per repeated execution. */
static unsigned int run(int fd, unsigned int *retval)
{
    char data[64] = {0};
    union bpf_attr attr = {0};
    attr.test.prog_fd = fd;
    attr.test.data_in = (uintptr_t)data;
    attr.test.data_size_in = sizeof(data);
    attr.test.repeat = REPEAT;
    if (syscall(__NR_bpf, BPF_PROG_TEST_RUN, &attr, sizeof(attr)))
        stop("BPF_PROG_TEST_RUN");
    *retval = attr.test.retval;
    if (*retval != 7) {
        fprintf(stderr, "Expected 7, got %u\n", *retval);
        exit(1);
    }
    return attr.test.duration;
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3 ||
        (strcmp(argv[1], "original") && strcmp(argv[1], "modified"))) {
        fprintf(stderr, "Usage: %s original|modified [load-samples]\n", argv[0]);
        return 1;
    }
    unsigned int samples = argc == 3 ? strtoul(argv[2], NULL, 10) : 101;
    if (!samples || samples > 10000) {
        fprintf(stderr, "Use 1 to 10000 load samples\n");
        return 1;
    }
    struct utsname u;
    if (uname(&u)) stop("uname");
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("# kernel=%s mode=%s load_samples=%u run_samples=%u repeat=%u warmup=10\n",
           u.release, argv[1], samples, RUN_SAMPLES, REPEAT);
    unsigned int sizes[] = {1, 8, 64, 256, 1024};
    add("control", 0);
    for (unsigned int i = 0; i < 5; i++) {
        add("mov", sizes[i]);
        if (!strcmp(argv[1], "modified")) add("nop", sizes[i]);
    }
    puts("phase,round,kind,extra,insns,load_ns,verify_us,processed,duration_ns,retval");
    for (int round = -10; round < (int)samples; round++) {
        /* Rotate the first case each round to spread ordering effects. */
        for (unsigned int j = 0; j < count; j++) {
            unsigned int i = (j + round + 10) % count;
            struct test *t = &tests[i];
            uint64_t elapsed;
            unsigned long long verify;
            unsigned int processed;
            int fd = load(t, &elapsed, &verify, &processed);
            if (round == 0) {
                printf("# verifier,%s,%u\n%s", t->name, t->extra, logbuf);
                inspect(fd, t);
            }
            if (round >= 0)
                printf("load,%d,%s,%u,%u,%llu,%llu,%u,0,0\n", round,
                       t->name, t->extra, t->count,
                       (unsigned long long)elapsed, verify, processed);
            close(fd);
        }
    }
    int fds[11];
    for (unsigned int i = 0; i < count; i++) {
        uint64_t elapsed;
        unsigned long long verify;
        unsigned int processed;
        fds[i] = load(&tests[i], &elapsed, &verify, &processed);
        inspect(fds[i], &tests[i]);
    }
    for (int round = -3; round < RUN_SAMPLES; round++) {
        for (unsigned int j = 0; j < count; j++) {
            unsigned int i = (j + round + 3) % count;
            unsigned int retval;
            unsigned int duration = run(fds[i], &retval);
            if (round >= 0)
                printf("run,%d,%s,%u,%u,0,0,0,%u,%u\n", round,
                       tests[i].name, tests[i].extra, tests[i].count, duration, retval);
        }
    }
    for (unsigned int i = 0; i < count; i++) close(fds[i]);
    puts("# benchmark complete");
    return 0;
}
