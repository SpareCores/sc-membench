# Changelog

## Unreleased

No release has been published since 1.2.2, and `membench.c` has not changed,
so the `:main` image runs 1.2.2 code.

### Behavior Checked Against the Code

The code of 1.2.2 behaves as follows, where earlier descriptions of the tool,
including its `-h` usage text, said otherwise:

- `bandwidth_mb_s` is computed in MiB/s (bytes divided by 1,048,576), not
  MB/s.
- Aggregate bandwidth is the total bytes divided by the time of the slowest
  thread, not the sum of per-thread bandwidths.
- `OMP_NUM_THREADS` has no effect, and `OMP_PROC_BIND` is overridden by the
  `proc_bind(spread)` clause in the code; `OMP_PLACES` still applies.
- With `libnuma`, multi-thread bandwidth buffers come from
  `numa_alloc_onnode` and bypass `-H`; `-H` applies to latency buffers and
  one-thread runs.
- The image is built without `libhugetlbfs` and reads the huge page size from
  `/proc/meminfo`.
- The default sweep stops at 4 × L3, not at 512 MB per thread, as the usage
  text says.
- The `-H` threshold is twice the detected huge page size, not ">= 4MB", as
  the usage text says.
- `-t` is checked only after each measurement and is ignored with `-s`.
- Latency sampling stops at 21 samples whatever the CV.
- Plain `make` detects and links `hwloc`, `libnuma`, and `libhugetlbfs`;
  `make basic` is the minimal build.
- `--privileged` is needed for the `nice -n -20` entrypoint.
- `-V` prints the version and exits.

On macOS, CI builds with Homebrew LLVM clang, because Apple clang ships no
OpenMP; the help text of the `Makefile` still names
`brew install hwloc libomp`, for a build with Apple clang.

### Multi-Platform CI

CI builds every variant and runs a smoke test on Linux x86_64 and arm64,
macOS arm64 and x86_64, and FreeBSD x86_64 (2026-08-27). A `.gitignore` for
the built binaries was added.

### Compressed Layers and resource-tracker

The image layers are zstd-compressed, and the image copies in the
`resource-tracker` binary (2026-06-10).

### Arm64 Runner and Action Upgrades

The `linux/arm64` image is built on a native Arm64 runner, and the GitHub
Actions were upgraded (2026-04-23).

### Docker Image Build

The `Dockerfile` and the image workflow were added (2026-02-27), moved from
`images/membench/` in the `sc-images` repository, where the image had been
built since 2026-01-05. A push to `main` publishes
`ghcr.io/sparecores/membench:main` and `:latest` for `linux/amd64` and
`linux/arm64`; a published release also publishes its version tag.

## 1.2.2: Man Page

Released 2026-02-27. Added the `membench.1` man page, which `make install`
installs. `membench.c` differs from 1.2.1 only in its version. The release
also holds the following changes:

- For issue [#2](https://github.com/SpareCores/sc-membench/issues/2), the
  `Makefile` wraps the header-file fallback of the `libnuma` and
  `libhugetlbfs` detection in parentheses (`8b42a20`).
- For issue [#3](https://github.com/SpareCores/sc-membench/issues/3), the
  release links the packaging status of sc-membench on repology.org
  (`c4b695f`).

## 1.2.1: Universal Binary

Released 2026-01-28. The build targets the baseline instruction set of each
architecture (`-march=x86-64 -mtune=generic` on x86_64, `-mcpu=generic+crc`
on aarch64), so one binary runs on every CPU of the architecture.

## 1.2.0: Human-Readable Output and Scores

Released 2026-01-26. Added `-R`, which prints a table, a summary, and the
Bandwidth, Latency, and Combined scores instead of CSV, after user feedback.

## 1.1.0: OpenMP

Released 2026-01-13. The bandwidth tests were rewritten to OpenMP.

## 1.0.3: Multi-Platform and New Latency Benchmark

Released 2026-01-13. This release made the following changes:

- a new latency test adapted from `ram_bench`: a pointer chase over nodes in
  random order
- support for platforms other than Linux, and an overridable `CC`
- huge page size detection with `libhugetlbfs`

## 1.0.2: More Consistent Results

Released 2026-01-11. Each measurement runs until either a target time or a
maximum iteration count is reached.

## 1.0.1: Huge Pages Support

Released 2026-01-10. Added `-H` for huge pages and the selection of
operations.

## 1.0.0: Initial Release

Released 2026-01-05. The first version had the following features:

- cache sizes detected with `hwloc`, and buffer sizes derived from them
- per-thread buffers, thread pinning, and NUMA memory binding
- best-of tries, a single-size mode, and an unlimited default runtime
- copy bandwidth matched to the convention of `bw_mem`
- a latency test that ends adaptively

## Experiments That Informed the Design

The measurements in this section were taken with earlier versions, and which
version and settings produced them was not stated.

### Huge Pages and Latency

With `-H`, latency measurements were often 20-40% lower while bandwidth
stayed roughly the same.

Measured on [Azure D96pls_v6](https://sparecores.com/server/azure/Standard_D96pls_v6)
(96 ARM Neoverse-N2 cores, 2 NUMA nodes, L1d=64KB/core, L2=1MB/core,
L3=128MB shared):

| Buffer | No Huge Pages | With THP (-H) | Improvement |
|--------|---------------|---------------|-------------|
| 32 KB  | 1.77 ns | 1.77 ns | HP not used (< 4MB) |
| 128 KB | 3.95 ns | 3.95 ns | HP not used (< 4MB) |
| 512 KB | 5.99 ns | 5.98 ns | HP not used (< 4MB) |
| 1 MB   | 11.52 ns | 10.92 ns | HP not used (< 4MB) |
| 2 MB   | 24.27 ns | 24.65 ns | HP not used (< 4MB) |
| 32 MB | 44.90 ns | 36.23 ns | -19% |
| 64 MB | 49.40 ns | 40.77 ns | -17% |
| 128 MB | 92.50 ns | 78.32 ns | -15% |
| 256 MB | 121.92 ns | 107.65 ns | -12% |
| 512 MB | 140.97 ns | 118.74 ns | -16% |

Measured on [AWS c8a.metal-48xl](https://sparecores.com/server/aws/c8a.metal-48xl)
(192 AMD EPYC 9R45 cores, 2 NUMA nodes, L1d=48KB/core, L2=1MB/core,
L3=32MB/die):

| Buffer | No Huge Pages | With THP (-H) | Improvement |
|--------|---------------|---------------|-------------|
| 32 KB  | 0.89 ns | 0.89 ns | HP not used (< 4MB) |
| 128 KB | 2.43 ns | 2.45 ns | HP not used (< 4MB) |
| 512 KB | 3.32 ns | 3.35 ns | HP not used (< 4MB) |
| 1 MB   | 5.47 ns | 4.09 ns | HP not used (< 4MB) |
| 2 MB   | 8.85 ns | 8.85 ns | HP not used (< 4MB) |
| 8 MB | 11.72 ns | 10.32 ns | -12% |
| 16 MB | 12.58 ns | 10.74 ns | -15% |
| 32 MB | 30.83 ns | 11.29 ns | -63% |
| 64 MB | 84.81 ns | 75.25 ns | -11% |
| 128 MB | 117.75 ns | 105.45 ns | -10% |

Key observations:

- Small buffers (≤ 2MB): no significant difference; the TLB can handle the
  page count.
- L3 boundary effect: AMD shows 63% improvement at 32MB (exactly at L3 size).
  Without huge pages, TLB misses make L3 appear like RAM.
- L3 region: 12-19% improvement with huge pages.
- RAM region: 10-16% lower latency with huge pages.
- THP works automatically: no pre-allocation needed.

At the time, the difference was explained as follows. With 4KB pages,
a 128 MB buffer spans 32,768 pages, which cannot fit in a TLB of \~1000-2000
entries, so random pointer chasing causes frequent TLB misses, each adding
10-20+ CPU cycles for a page table walk. With 2MB huge pages, the same buffer
spans 64 pages. Bandwidth tests improve less because sequential access has
better TLB locality, hardware prefetchers hide TLB miss latency, and the
memory bus is already saturated.

That explanation does not apply to a build with `libnuma`: its multi-thread
bandwidth buffers bypass `-H`, so huge pages could not change their
bandwidth; see
[Behavior Checked Against the Code](#behavior-checked-against-the-code).

### Result Variability

With thread pinning, NUMA-local memory, best-of tries, and latency sampling,
the benchmark variability was stated as typically <1%, compared to 30-60%
without them.

### Example Output on Azure D96pls_v6

An earlier version produced the following output on
[Azure D96pls_v6](https://sparecores.com/server/azure/Standard_D96pls_v6)
(96 ARM cores, 64KB L1, 1MB L2, 128MB L3):

```csv
size_kb,operation,bandwidth_mb_s,latency_ns,latency_stddev_ns,latency_samples,threads,iterations,elapsed_s
32,read,9309701.64,0,0,0,96,292056,0.094113
32,write,9868845.93,0,0,0,96,578703,0.175918
32,latency,0,1.77,0.00,7,1,7,0.254053
128,read,6410473.70,0,0,0,96,83556,0.156412
128,write,9883443.78,0,0,0,96,177556,0.215580
128,latency,0,3.93,0.00,7,1,7,0.689736
512,latency,0,5.66,0.01,7,1,7,0.654846
1024,latency,0,7.38,0.04,7,1,7,0.671615
32768,latency,0,44.90,0.03,7,1,7,1.050579
131072,latency,0,96.78,3.00,7,1,7,8.520152
262144,latency,0,122.22,0.90,7,1,7,21.756578
```

The rows read as follows:

- 32KB: fits in L1, with very high bandwidth (\~9.3 TB/s read) and low
  latency (\~1.8ns, stddev 0.00)
- 512KB: fits in L2, with good latency (\~5.7ns, stddev 0.01)
- 32MB: in L3, with moderate latency (\~45ns, stddev 0.03)
- 128MB: at the L3 boundary, where RAM latency is visible (\~97ns, stddev 3.0)
- 256MB: past L3, with pure RAM latency (\~122ns, stddev 0.9)

### Example Human-Readable Output

An example of the `-R` output added in 1.2.0, from a server with 32 CPUs:

```text
Size       Op          Bandwidth      Latency  Threads
----       --          ---------      -------  -------
32 KB      read         2.6 TB/s            -       32
32 KB      write        1.6 TB/s            -       32
32 KB      copy       464.4 GB/s            -       32
32 KB      latency             -       0.9 ns        1
128 KB     read         1.7 TB/s            -       32
128 KB     write      691.7 GB/s            -       32
128 KB     copy       495.5 GB/s            -       32
128 KB     latency             -       2.4 ns        1
...

================================================================================
                           BENCHMARK SUMMARY
================================================================================

BANDWIDTH (MB/s):
  Operation          Peak Weighted Avg
  ---------          ---- ------------
  Read            2612561      1680432
  Write           1605601       850445
  Copy             495476       372027

LATENCY:
  Best latency: 97.2 ns (RAM) at 131072 KB buffer

--------------------------------------------------------------------------------
BENCHMARK SCORE (higher is better):

  Bandwidth Score:      1571.2  (avg peak bandwidth in GB/s)
  Latency Score:          10.3  (1000 / latency_ns)

  >> COMBINED SCORE:      4024  (sqrt(bw_score × latency_score) × 100)
--------------------------------------------------------------------------------
```

The table goes to stdout and the summary to stderr. The "MB/s" label of the
summary is MiB/s; see [Bandwidth Unit](./docs/limitations.md#bandwidth-unit).
The Combined Score of this example does not match the formula, which gives
about 12,720 for these two scores.

### Typical Values

The following typical values for modern systems were given at the time,
without a source:

| Level | Bandwidth | Latency |
| --- | --- | --- |
| L1 cache | 200-500 GB/s (varies with frequency) | 1-2 ns |
| L2 cache | 100-200 GB/s | 3-10 ns |
| L3 cache | 50-100 GB/s | 10-40 ns (larger/3D V-Cache may be higher) |
| Main memory | 20-100 GB/s (DDR4/DDR5, depends on channels) | 25-50 ns (fast DDR5) to 60-120 ns (DDR4) |

The L1 boundary was also placed at "32-64KB per thread typically" and the L2
boundary at "256KB-1MB per thread typically".
