# Test return value

The current test sources use 7. NOP continues, MOV puts 7 in R0 and EXIT returns it. The marker is arbitrary and does not change the instruction's behaviour.

The immediate values and expected results were changed in `nop/test.c`, `nop/check.c` and `nop/jit/check.c`. The kernel images were reused because no kernel source changed for this rerun.

The jump case also contains a MOV of 9 on the other path. Its condition compares a known 7 with 7, so the verifier can simplify that path before execution. This test checks acceptance and the final return value. It does not establish runtime conditional-branch coverage.

The archived first checker and older logs retain their original values, including 42. They were not edited to match the newer source.

## Recorded run

The userspace programs were recompiled on 26 October 2026. Each guest used one CPU, 2 GiB RAM and no network adapter.

| Run | Small NOP test | Checker |
|---|---|---|
| Smaller image, JIT off | Returned 7, status 0 | 20/20 |
| JIT-capable image, switch off before loading | Returned 7, status 0 | 20/20 |
| Same JIT-capable image, switch on | Returned 0, status 1 | 14/20 |

The first two runs used the interpreter. The JIT-on run still failed for all six valid NOP programs. Its control returned 7 and had JIT code. The 14/20 result comes from the JIT-specific checker.

The runtime-switch test restored the guest's setting to 1 after closing the programs. The host setting was not changed. The original kernel was not rerun with 7 in this sequence.

## Commands

The full host script is [return-7.sh](../session/return-7.sh). It assumes the earlier images and guest filesystem exist:

```sh
bash nop/session/return-7.sh
```

It compiles the three userspace programs:

```sh
gcc -static -Wall -Wextra -O2 nop/test.c -o nop/test
gcc -static -Wall -Wextra -O2 nop/check.c -o nop/check
gcc -static -Wall -Wextra -O2 nop/jit/check.c -o nop/jit/check
```

The script copies them into the guest shares and boots the three images. Its final success means that all runs matched their expected outcomes, including the known JIT failure. It does not mean JIT executed NOP correctly.

The command log prints the parent Git revision. The value changes were uncommitted during the run and committed later. The recorded source and executable hashes identify the tested files.

Records: [host commands](return-7-commands.log.gz), [smaller guest](return-7-minimal.log.gz), [runtime-switch guest](return-7-runtime.log.gz), [JIT-on guest](return-7-jit.log.gz), and [hashes](return-7-sha256.txt).
