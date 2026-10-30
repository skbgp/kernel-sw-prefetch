# Understanding test.c and check.c

These notes explain the saved sources in the NOP experiment. The programs are ordinary Linux C executables. Their instruction arrays contain the eBPF programs that the kernel verifies and executes. The C main() functions themselves are not submitted as eBPF bytecode.

## Why we put 7 in R0

NOP has no effect on eBPF registers or memory. It still advances to the next instruction. MOV R0,7 supplies a known return value, and EXIT returns that value. A program containing only NOP and EXIT has not initialised R0, so the verifier rejects it. The value 7 is an arbitrary marker. It is not part of NOP's operation or encoding.

The small NOP-first program checks loading and the forward execution path. Because MOV writes R0 after NOP, that program alone cannot show preservation of an earlier R0 value. The checker also tests MOV R0,7 followed by NOP and EXIT, along with preservation of R2 and a stack value.

## C notation used below

The dot in attr.prog_type accesses a field of a variable. The arrow in t->count accesses a field through a pointer. The ampersand in &attr takes its address. A cast such as (uintptr_t) converts a pointer to an integer representation. An aggregate initialiser such as { .code = 0xe7 } sets the named field and zero-initialises omitted fields. This is why the reserved NOP fields are zero even when they are not written out.

The | operator combines bits. The || operator means logical OR, && means logical AND, and ! means logical NOT. Logical expressions produce 0 or 1. The expression condition ? a : b chooses a when the condition is true and b otherwise.

The three return-related quantities are different. BPF_PROG_LOAD returns a program file descriptor on success. BPF_PROG_TEST_RUN writes the eBPF program's result into attr.test.retval. The ordinary C main() returns a shell exit status, zero for a passing test and one for failure.


## test.c

Source SHA-256: `1d3f1f9397632f5509b7b7f0b6a595a2f7eb8eb2eb8e8346ceed2c75b576767c`. Line numbers below refer to this exact source. Blank lines separate related statements and have no effect on execution.

```c
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
```

**Line 1:** Defines the GNU feature-test macro before the headers are read. On Linux with glibc, this exposes declarations such as syscall(). It does not execute anything.

**Line 2:** Includes Linux BPF definitions, including struct bpf_insn, union bpf_attr, register identifiers, instruction constants and BPF commands.

**Line 3:** Includes fixed-width integer types and uintptr_t. We use uintptr_t to convert userspace pointers into an integer representation for BPF address fields.

**Line 4:** Includes declarations for printf(), puts() and perror(). These functions print normal output or errors.

**Line 5:** Includes system-call number definitions used to identify the BPF system call with __NR_bpf.

**Line 6:** Includes declarations for syscall() and close(). syscall() enters the requested kernel interface; close() releases a file descriptor.

**Line 8:** Names the complete opcode byte 0xe7 as NOP_CODE. This userspace definition is different from the kernel operation-field definition BPF_NOP = 0xe0. Our test sends the complete E7 byte.

**Line 10:** Defines main(), the entry function of this ordinary C executable. It takes no command-line arguments and returns an integer exit status to the shell.

**Line 11:** Opens the body of main().

**Line 12:** Creates an array named program. Each element is one struct bpf_insn. This array contains the eBPF instructions that will be submitted to the kernel.

**Line 13:** Initialises the first instruction as E7, our experimental NOP. Because this is an aggregate initialiser, the omitted register, offset and immediate fields are zero. This matches the required NOP encoding.

**Line 14:** Starts the second instruction. BPF_ALU64 selects the 64-bit ALU class, BPF_MOV selects a move, and BPF_K selects an immediate source. Bitwise OR combines these fields into opcode 0xb7.

**Line 15:** Completes the second instruction by choosing R0 as the destination and 7 as the immediate. Together, lines 14 and 15 mean R0 = 7. This gives the program an expected return value.

**Line 16:** Adds EXIT. BPF_JMP identifies the instruction class and BPF_EXIT identifies the exit operation. Their combined opcode is 0x95. EXIT finishes the eBPF program and returns the value in R0.

**Line 17:** Closes the instruction array. It contains three instructions, each eight bytes, so the array occupies 24 bytes on the target platform.

**Line 18:** Creates a 65,536-byte character array for verifier messages and initialises it to zero. The initial zero bytes also let the program safely print an empty string if the kernel supplies no diagnostic.

**Line 19:** Creates and zero-initialises the BPF request variable attr. It is a union because different BPF commands use different layouts within this request. Starting from zero avoids accidentally passing unrelated values in unused fields.

**Line 21:** Sets the program type to a socket filter. We chose a type supported by the test-run interface. The program is not attached to a socket in this test.

**Line 22:** Calculates the number of instructions by dividing the size of the whole array by the size of one element. Here this gives 24 / 8 = 3. This avoids writing an instruction count that could become wrong if the array changes.

**Line 23:** Stores the userspace address of the instruction array in attr.insns. The kernel uses this address to copy the submitted instructions. It does not execute a pointer to our C main() function.

**Line 24:** Passes the address of the GPL licence string. This is the licence declaration supplied with the BPF load request.

**Line 25:** Passes the userspace address of the verifier-log buffer.

**Line 26:** Tells the kernel how much space the log buffer has. sizeof(log) is 65,536 bytes.

**Line 27:** Requests verifier logging with log level 1.

**Line 29:** Calls the BPF system call with command BPF_PROG_LOAD. &attr is the address of the request, and sizeof(attr) is the request size, not the instruction-array size. On success the result is a nonnegative program file descriptor, saved in fd. On failure syscall() returns -1 and sets errno. Loading includes verification and runtime preparation; it does not run our three instructions.

**Line 30:** Checks whether loading failed. If fd is negative, the statements inside this block run.

**Line 31:** Prints the supplied prefix and the operating-system error description to standard error. For example, it may print Load failed: Invalid argument. This is separate from the detailed verifier log.

**Line 32:** Prints the verifier log to standard output. This is where a message such as unknown opcode e7 can appear.

**Line 33:** Ends the C test with shell exit status 1 because loading failed. No program descriptor was obtained, so none needs to be closed here.

**Line 34:** Closes the load-failure block.

**Line 35:** Prints Program loaded after a successful load. This message alone does not establish correct execution.

**Line 37:** A source-code comment explaining the test setup. The test supplies a local buffer and sends no packet through a network interface.

**Line 38:** Creates a 64-byte input buffer filled with zero bytes. The socket-filter test-run command uses this as test input. Our particular instructions do not read its contents.

**Line 39:** Clears the request again by assigning a zero-initialised union value. The next command uses attr.test fields rather than the program-load fields, so old values must not carry over.

**Line 40:** Identifies which loaded program to execute by putting fd into the test request.

**Line 41:** Passes the userspace address of the input buffer.

**Line 42:** Passes the input-buffer length, which is 64 bytes.

**Line 43:** Requests one test iteration.

**Line 44:** Calls BPF_PROG_TEST_RUN and checks whether that system call failed. A successful test-run system call returns zero; the eBPF return value is placed separately in attr.test.retval.

**Line 45:** If test execution failed, prints the operating-system error description.

**Line 46:** Closes the program file descriptor on the execution-error path.

**Line 47:** Ends the C test with shell exit status 1 because execution could not be completed.

**Line 48:** Closes the execution-error block.

**Line 49:** Prints the value returned by the eBPF program. %u formats an unsigned integer, and the newline ends the output line.

**Line 50:** Uses the conditional operator to select the C test result. If attr.test.retval equals 7, result becomes 0; otherwise result becomes 1. This detects the recorded JIT-on outcome where the program loaded but returned zero.

**Line 51:** Closes the program descriptor after the return-value check. This test creates no attachment or other persistent reference to the program.

**Line 52:** Returns the C test result to the shell. echo $? immediately afterwards shows 0 for success or 1 for failure. This shell result is different from the eBPF return value 7.

**Line 53:** Closes main().


## check.c

Source SHA-256: `4f3972fe22f95c790c922ae0f6f103f5f360a767b4c0ad404fb9b5381032b9c2`. Line numbers below refer to this exact source. Blank lines separate related statements and have no effect on execution.

```c
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
        { "control", 2, 1, { MOV(0, 7), EXIT } },
        { "NOP first", 3, nop, { NOP, MOV(0, 7), EXIT } },
        { "NOP preserves r0", 3, nop, { MOV(0, 7), NOP, EXIT } },
        { "two NOPs", 4, nop, { MOV(0, 7), NOP, NOP, EXIT } },
        { "NOP preserves r2", 4, nop, { MOV(2, 7), NOP,
            { .code = BPF_ALU64 | BPF_MOV | BPF_X, .dst_reg = 0, .src_reg = 2 }, EXIT } },
        { "NOP preserves stack", 4, nop, {
            { .code = BPF_ST | BPF_MEM | BPF_DW, .dst_reg = 10, .off = -8, .imm = 7 }, NOP,
            { .code = BPF_LDX | BPF_MEM | BPF_DW, .dst_reg = 0, .src_reg = 10, .off = -8 }, EXIT } },
        { "jump to NOP", 5, nop, { MOV(0, 7),
            { .code = BPF_JMP | BPF_JEQ | BPF_K, .dst_reg = 0, .off = 1, .imm = 7 }, MOV(0, 9), NOP, EXIT } },
        { "uninitialized return", 2, 0, { NOP, EXIT } },
        { "uninitialized read after NOP", 3, 0, { NOP,
            { .code = BPF_ALU64 | BPF_MOV | BPF_X, .dst_reg = 0, .src_reg = 2 }, EXIT } },
        { "missing exit", 2, 0, { MOV(0, 7), NOP } },
        { "dst 1", 3, 0, { { .code = 0xe7, .dst_reg = 1 }, MOV(0, 7), EXIT } },
        { "dst 15", 3, 0, { { .code = 0xe7, .dst_reg = 15 }, MOV(0, 7), EXIT } },
        { "src 1", 3, 0, { { .code = 0xe7, .src_reg = 1 }, MOV(0, 7), EXIT } },
        { "offset 1", 3, 0, { { .code = 0xe7, .off = 1 }, MOV(0, 7), EXIT } },
        { "offset -1", 3, 0, { { .code = 0xe7, .off = -1 }, MOV(0, 7), EXIT } },
        { "immediate 1", 3, 0, { { .code = 0xe7, .imm = 1 }, MOV(0, 7), EXIT } },
        { "immediate -1", 3, 0, { { .code = 0xe7, .imm = -1 }, MOV(0, 7), EXIT } },
        { "ALU32 encoding e4", 3, 0, { { .code = 0xe4 }, MOV(0, 7), EXIT } },
        { "register encoding ef", 3, 0, { { .code = 0xef }, MOV(0, 7), EXIT } },
        { "unsupported f7", 3, 0, { { .code = 0xf7 }, MOV(0, 7), EXIT } },
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
            ok = t->accept && info_ok && !info.jited_prog_len && run_ok && attr.test.retval == 7;
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
```

**Line 1:** Enables the GNU declarations needed by the program, including syscall(), before including headers.

**Line 2:** Includes errno and error constants such as EINVAL and EACCES. We use these when checking expected load failures.

**Line 3:** Includes the Linux BPF request, instruction and program-information definitions.

**Line 4:** Provides uintptr_t for representing userspace addresses in BPF request fields.

**Line 5:** Provides printf() and fprintf() for results and usage messages.

**Line 6:** Provides strcmp() for comparing command-line strings and strerror() for converting error numbers to readable messages.

**Line 7:** Provides BPF system-call number definitions.

**Line 8:** Provides syscall() and close().

**Line 10:** Defines NOP as a struct bpf_insn initialiser containing opcode E7. The other fields become zero when used in these aggregate initialisers. This macro describes an instruction; it does not execute NOP in userspace.

**Line 11:** Defines MOV(r, v), a macro that constructs an immediate 64-bit move instruction. For example, MOV(2, 7) expands to an instruction that writes 7 into R2. The | operators combine opcode fields.

**Line 12:** Defines EXIT as an instruction initialiser for BPF_JMP | BPF_EXIT, whose opcode is 0x95.

**Line 14:** Starts a userspace record type named struct test. This is the checker's own description of a test case, separate from Linux's struct bpf_insn.

**Line 15:** Stores the test name as a pointer to characters that the program will not modify. The name is used in printed results.

**Line 16:** Stores how many instruction slots in this test should be submitted.

**Line 17:** Stores the expected load outcome. A value of 1 means the program should load, and 0 means it should be rejected.

**Line 18:** Reserves space for up to eight eBPF instructions in each test record. Only count instructions are submitted. Unused array elements are not added to the BPF program.

**Line 19:** Closes the struct test type definition.

**Line 21:** Defines main() with command-line arguments. argc counts the arguments, including the executable name. argv contains pointers to their strings.

**Line 22:** Opens main().

**Line 23:** Rejects an invalid command line. There must be two arguments in total: the executable name and one mode. strcmp() returns zero when strings are equal, so the expression inside parentheses is true only when the mode differs from both original and modified. The || short-circuits: if argc is wrong, the argv[1] comparisons are not evaluated.

**Line 24:** Prints the required command form to standard error. argv[0] supplies the executable name for %s. The | in the usage message means choose either original or modified; it is only text here.

**Line 25:** Returns shell exit status 1 for incorrect usage.

**Line 26:** Closes the usage-error block.

**Line 27:** Sets nop to 1 for modified mode and 0 for original mode. strcmp(argv[1], "modified") returns zero for a match, and ! converts that zero to 1. This variable controls the expected outcome of valid NOP programs. It does not change the running kernel.

**Line 28:** Begins the array of test records. Each record supplies a name, an instruction count, an expected-acceptance value and an instruction array.

**Line 29:** Defines the control program: MOV R0,7 followed by EXIT. It contains no new instruction and should load in both modes, so its accept field is always 1.

**Line 30:** Defines the three-instruction NOP-first program. Its accept field is nop, so rejection is expected on the original kernel and acceptance on the modified kernel.

**Line 31:** Sets R0 to 7 before NOP and then exits. The accepted program must still return 7, checking that NOP preserves the earlier R0 value in this case.

**Line 32:** Places two consecutive NOP instructions between setting R0 and exiting. This checks that execution can continue through repeated NOPs while retaining the expected result.

**Line 33:** Starts a four-instruction case that stores 7 in R2 and executes NOP. R2 holds a value that should remain available afterwards.

**Line 34:** Completes that case by copying R2 into R0 and exiting. BPF_X selects a register source, dst_reg = 0 selects R0, and src_reg = 2 selects R2. The expected return is 7.

**Line 35:** Starts the four-instruction stack-preservation case.

**Line 36:** Stores the immediate value 7 as a 64-bit value at R10 - 8, then executes NOP. BPF_ST selects an immediate store, BPF_MEM selects memory addressing and BPF_DW selects an eight-byte access. R10 is the BPF stack frame pointer, and -8 addresses an eight-byte slot below it.

**Line 37:** Loads the eight-byte value from R10 - 8 into R0 and exits. BPF_LDX selects a memory load. Returning 7 checks the stored value across NOP in this program.

**Line 38:** Starts a five-instruction jump-layout case by setting R0 to 7.

**Line 39:** Compares R0 with immediate 7. If equal, off = 1 skips the following MOV R0,9 and reaches NOP. Jump offsets are relative to the next instruction. However, R0 is already known to equal 7, so the verifier can simplify the branch and remove the unused path. The test checks acceptance and the final return value; it does not prove runtime conditional-branch coverage.

**Line 40:** Defines NOP followed immediately by EXIT without initialising R0. It must be rejected on the modified kernel because NOP cannot make R0 readable. The original kernel rejects the unknown E7 before reaching that state check.

**Line 41:** Starts a case containing NOP followed by a read from a register that has not been initialised.

**Line 42:** Copies uninitialised R2 into R0 and exits. Verification must reject it on the modified kernel. This checks that adding NOP does not make the unsafe register read acceptable.

**Line 43:** Defines MOV R0,7 and NOP with no EXIT. It must be rejected because execution would fall beyond the program.

**Line 44:** Submits E7 with dst_reg = 1. Our NOP requires this reserved field to be zero, so the modified kernel must reject it.

**Line 45:** Submits E7 with dst_reg = 15. This also violates NOP's reserved-field rule. A four-bit field can encode 15, but that does not make 15 a valid BPF register number.

**Line 46:** Submits E7 with src_reg = 1. This field is unused by our NOP and must be zero, even though the opcode uses the immediate-source form.

**Line 47:** Submits E7 with positive offset 1. The offset is reserved for NOP, so this must be rejected.

**Line 48:** Submits E7 with negative offset -1. This must also be rejected; the rule requires zero, not merely a nonpositive or small offset.

**Line 49:** Submits E7 with immediate 1. The immediate is reserved for NOP, so this must be rejected.

**Line 50:** Submits E7 with immediate -1. This must also be rejected.

**Line 51:** Submits E4, the E0 operation field combined with the ALU32 class. Our experiment registers only E7, so the early opcode-membership check rejects this alternative encoding.

**Line 52:** Submits EF, the E0 operation field combined with the ALU64 class and BPF_X register-source bit. This form is not registered, so the early opcode-membership check rejects it.

**Line 53:** Submits F7, an unsupported F0 operation in the ALU64 immediate form. It must remain rejected after adding E7.

**Line 54:** Closes the array containing all twenty test records.

**Line 55:** Initialises the number of passing cases to zero. unsigned int stores a nonnegative integer count.

**Line 56:** Loops over every test. i starts at zero, the sizeof division calculates the array length, and i++ advances to the next case after each iteration.

**Line 57:** Sets t to point to the current test record. &tests[i] takes its address. Expressions such as t->count access a field through that pointer.

**Line 58:** Creates a fresh zeroed verifier-log buffer for the current case. Reinitialising it prevents a previous case's diagnostic from being mistaken for this case's log.

**Line 59:** Creates a fresh zeroed BPF request for this case.

**Line 60:** Selects the socket-filter program type for all twenty cases.

**Line 61:** Uses the current record's count field as the submitted instruction count.

**Line 62:** Passes the address of the current test's instruction array. Only the count specified on line 61 is submitted, rather than all eight reserved array elements.

**Line 63:** Supplies the address of the GPL licence declaration.

**Line 64:** Supplies the current case's verifier-log address.

**Line 65:** Supplies the log-buffer size.

**Line 66:** Requests verifier logging.

**Line 67:** Attempts to load the current eBPF program. A nonnegative fd identifies a loaded program; -1 indicates failure.

**Line 68:** Immediately saves errno, which describes a failed system call. Saving it now prevents later library calls from changing the error we want to inspect. If loading succeeded, this saved errno is not used to judge the case.

**Line 69:** Initialises this case's result to failure. Later code sets ok to 1 only if the expected conditions are satisfied.

**Line 70:** Enters the rejection path if loading failed.

**Line 71:** Counts rejection as expected only if the case should reject, the error is EINVAL or EACCES, and the log has at least one character. !t->accept checks expected rejection. log[0] is nonzero only when the zero-initialised buffer now contains text. This does not check the exact diagnostic, so an unrelated verifier rejection could satisfy this condition.

**Line 72:** Prints the test name, readable saved error and verifier log. The three %s substitutions receive t->name, strerror(error) and log.

**Line 73:** Otherwise, enters the loaded-program path. Any program that loaded enters this path, including a case that was expected to reject; line 87 will then mark that unexpected acceptance as failure.

**Line 74:** Creates and zeroes a struct bpf_prog_info variable. The kernel can fill it with information about the loaded program.

**Line 75:** Clears attr before using its info-command layout.

**Line 76:** Specifies the loaded program descriptor whose information is requested.

**Line 77:** Gives the size of our information buffer.

**Line 78:** Gives the userspace address of that information buffer. &info takes its address; the cast converts it into an integer representation for the request.

**Line 79:** Calls BPF_OBJ_GET_INFO_BY_FD. info_ok becomes 1 if the call returns zero, or 0 otherwise. On success, fields in info describe the loaded program.

**Line 80:** Creates a fresh 64-byte, zero-filled input buffer for execution. No network transmission is performed.

**Line 81:** Clears attr again before using its test-run layout.

**Line 82:** Chooses the loaded program to execute.

**Line 83:** Provides the address of its local input buffer.

**Line 84:** Provides the input-buffer size.

**Line 85:** Requests one test iteration.

**Line 86:** Calls BPF_PROG_TEST_RUN. run_ok becomes 1 if this system call succeeds. The program's own return value is placed separately in attr.test.retval.

**Line 87:** Counts a loaded case as passing only when all five conditions hold: acceptance was expected, the info call succeeded, the JIT code length is zero, execution succeeded and the returned value is 7. The && operators require every condition to be true. !info.jited_prog_len means the native JIT code length is zero. That alone would not prove correct interpreter execution, which is why execution and the expected return value are also checked.

**Line 88:** Prints the case name, eBPF return value and reported JIT code length. Check the final PASS or FAIL result too, because these printed fields alone do not establish that both system calls succeeded.

**Line 89:** Closes the loaded program descriptor before the next case.

**Line 90:** Closes the loaded-program branch.

**Line 91:** Prints PASS when ok is 1 and FAIL when it is 0, followed by the test name. The conditional operator chooses the output string.

**Line 92:** Adds ok to the passing-case count. The logical conditions used above produce 0 or 1, so failed cases add zero and passing cases add one.

**Line 93:** Closes the loop body. Execution moves to the next test until all cases have been handled.

**Line 94:** Calculates the total number of records, which is twenty.

**Line 95:** Prints the number of passing cases and the total, for example Checks: 20/20 passed.

**Line 96:** Returns shell exit status 0 if every case passed. If at least one case failed, it returns 1.

**Line 97:** Closes main().


## What the twenty outcomes mean

In modified mode, the control and six valid NOP programs should load, execute and return 7. The remaining thirteen should be rejected. In original mode, only the control should load; the other nineteen contain E7 or another unsupported encoding and should reject. Thus both kernels can show 20/20 while accepting different programs.

The mode argument changes the checker's expectations. It does not modify or select the running kernel. Always check uname -r inside the guest before interpreting the result.

This check.c is the interpreter checker and requires jited_prog_len to be zero for all accepted cases. The separate jit/check.c has different expectations for the control program. The recorded JIT-on result of 14/20 belongs to that JIT-specific checker, not to this regular interpreter checker.

## Limits of these tests

The jump case uses a known constant, so it does not establish runtime conditional-branch coverage. The negative tests accept either EINVAL or EACCES with any nonempty diagnostic, so they do not identify the exact rejection check. A zero JIT code length by itself does not prove execution went through the intended interpreter handler. These tests also check the execution status and return value against the known source implementation. Twenty cases provide focused evidence and do not prove correctness for all possible programs.

## Compile and run

Compile on the x86-64 Linux host from the kernel repository directory:

```sh
gcc -static -Wall -Wextra -O2 nop/test.c -o nop/test
gcc -static -Wall -Wextra -O2 nop/check.c -o nop/check
```

These commands compile the userspace harnesses. Building the modified kernel is a separate step. In the recorded setup, QEMU shares the host's nop directory as /shared, so the guest can execute these files there.

Inside the modified QEMU guest, with the interpreter selected before loading:

```sh
uname -r
/shared/test
echo $?
/shared/check modified
echo $?
```

The small test should print Program loaded and Return value: 7, then exit with zero. The checker should report 20/20 and exit with zero. Inside the original guest, /shared/test should reject E7 and exit with one, while /shared/check original should count the expected outcomes and exit with zero.

These notes explain the saved programs. No kernel build or runtime test was performed while writing them.
