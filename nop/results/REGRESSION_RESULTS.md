# Existing BPF regression tests

| Suite | Original passed | Original failed | Modified passed | Modified failed |
|---|---:|---:|---:|---:|
| BPF tests | 1053 | 0 | 1053 | 0 |
| Tail calls | 10 | 0 | 10 | 0 |
| skb segmentation | 2 | 0 | 2 | 0 |

Original kernel: `6.15.0-g680b721a3232`.
Modified kernel: `6.15.0-gc9d2947cd680`.

Both used byte-identical configurations with CONFIG_TEST_BPF=m and CONFIG_BPF_JIT=n. Each module was built from its own kernel tree. The kernel image hashes remained identical to those used in the focused NOP tests. QEMU had one virtual CPU, 2048 MiB RAM and no network device. Module insertion and removal returned 0 on both kernels.

The full raw logs are regression-original-guest.log and regression-modified-guest.log. The summary excerpts are labeled extracts, not screenshots. These results cover the test_bpf module suites; they do not establish that the full BPF selftest suite or syzkaller has been run. No performance conclusion is drawn from test timing values.
