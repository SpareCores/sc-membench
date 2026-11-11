# membench

This benchmark measures the memory bandwidth and latency of cloud servers with
`membench`, a C tool that the Spare Cores team wrote with OpenMP. It sweeps
per-thread buffer sizes derived from the detected L1d, L2, and L3 cache sizes,
from half the L1d cache to four times the L3 cache, so that the results trace
each cache level and RAM.

At each size, this benchmark reports the aggregate read, write, and copy
bandwidth of all CPUs, and the latency of a single thread chasing pointers
through the buffer in random order, as one CSV (Comma-Separated Values) row
per size and operation. It measures CPU memory local to the NUMA (Non-Uniform
Memory Access) node of each thread, not access across NUMA nodes, per-thread or
per-node bandwidth, or GPU memory, which
[`nvbandwidth`](https://github.com/SpareCores/sc-images/tree/main/images/nvbandwidth)
measures.

## Purpose

[Spare Cores Navigator](https://docs.sparecores.com/overview) publishes
empirical performance measurements for more than 5,000 cloud server types,
among them the
[memory bandwidth](https://github.com/SpareCores/sc-images/blob/main/images/benchmark/docs/bw-mem.md)
measured with LMbench's `bw_mem`.

We ran `bw_mem` across 3000+ cloud server types, and the results were not
always consistent with the detected L1d, L2, and L3 cache sizes. Some
inconsistencies came from cache detection, which relied on the output of the
`lscpu` command, and that output is sometimes wrong or incomplete; others
traced back to `bw_mem` itself, so we wrote our own tool. The problems with
`bw_mem` were the following:

- Its `rd` mode reads every fourth word, so only 25% of the buffer is
  accessed, and the reported bandwidth looks about 4x higher than the actual
  throughput unless its `frd` mode is used.
- It forks one process per parallel worker instead of starting threads, and
  on servers with 96 or 192 vCPUs we saw unexpected slowdowns that we could
  not reproduce or explain. This benchmark uses OpenMP threads instead.
- It has no huge page support (it uses the `valloc` allocator, which returns
  regular pages), so large-buffer latency tests pay TLB (Translation Lookaside
  Buffer) overhead on top of the actual memory latency.
- It reports a single latency number, while this benchmark collects several
  independent samples per measurement and reports the median.

The article
[Modern Memory Bandwidth Benchmarks](https://sparecores.com/article/membench)
describes the `bw_mem` findings in full, and
[Comparison With LMbench](./docs/workloads.md#comparison-with-lmbench) lists
the differences in method.

The Navigator publishes four metrics from the
output of this benchmark: read, write, and copy bandwidth, and latency, each
per buffer size, plus one RAM value per metric. See
[How the Navigator Uses the Results](./docs/usage.md#how-the-navigator-uses-the-results)
for how the RAM value is picked.

## Limitations

The results compare the cache and memory subsystems across server types,
measured with portable, compiler-generated code. The main limitations are the
following:

- **[Plain C Loops](./docs/limitations.md#plain-c-loops):** The bandwidth
  loops are plain C for the baseline instruction set, without hand-written
  SIMD (Single Instruction, Multiple Data) code or non-temporal stores.
- **[Cache Detection](./docs/limitations.md#cache-detection):** The L3 size
  is that of one shared L3 cache, not the server total. When `hwloc` (Portable
  Hardware Locality) finds no caches, the sweep uses default cache sizes of
  32 KiB (L1d), 256 KiB (L2), and 8 MiB (L3), as on 17 Arm server types in the
  Navigator's [production runs](./docs/usage.md#production-setup).
- **[Container Limits](./docs/limitations.md#container-limits):** The CPU
  count and RAM size come from the host, not from Docker's `--cpus` or
  `--memory` limits.
- **[Bandwidth Unit](./docs/limitations.md#bandwidth-unit):** The
  `bandwidth_mb_s` column is in mebibytes per second (MiB/s), although its
  name and the Navigator label it MB/s.
- **[Aggregate Bandwidth](./docs/limitations.md#aggregate-bandwidth):** The
  bandwidth is the total bytes of all threads divided by the time of the
  slowest thread, not a sum of per-thread bandwidths.
- **[Thread Placement](./docs/limitations.md#thread-placement):** The
  `OMP_NUM_THREADS` and `OMP_PROC_BIND` variables do not change the bandwidth
  operations, and thread 0 stays on CPU 0 after the first latency test.
- **[Memory Guard](./docs/limitations.md#memory-guard):** At sizes where the
  buffers of all threads would exceed a quarter of the RAM, the thread count
  is halved until they fit, so the rows of one run can differ in `threads`.
- **[Latency Sampling](./docs/limitations.md#latency-sampling):** Sampling
  stops at 21 samples even when they still vary more than the 5% target, and
  latency is measured once per size, on CPU 0 only.
- **[Huge Pages Scope](./docs/limitations.md#huge-pages-scope):** In the
  image, the `-H` huge page option applies only to the latency chain and to
  bandwidth rows whose `threads` value is 1.
- **[Time Limit and Exit Code](./docs/limitations.md#time-limit-and-exit-code):**
  The `-t` time limit is checked only after each measurement, and the exit
  code is 0 even when a size could not be allocated.
- **[Older Production Results](./docs/limitations.md#older-production-results):**
  Results from versions 1.0.1 to 1.2.1 are published next to 1.2.2 results,
  with the version noted.

## Usage

Run the image via Docker. With no arguments, it sweeps the default sizes with
all four operations on all CPUs, and prints CSV to stdout:

```bash
docker run --rm --privileged ghcr.io/sparecores/membench:main
```

The `--privileged` option is required: the container entrypoint runs
`membench` under `nice -n -20`, which needs `CAP_SYS_NICE`. Arguments after
the image name go to `membench`, for example `-v` for a log on stderr. The
image is built for `linux/amd64` and `linux/arm64`, and `:main` is the tag to
use; it runs version 1.2.2 of the code.

To run the tool without Docker, check the
[packaging status](./docs/build.md#packages) for a distribution package, or
build it from source:

```bash
# Get the source
git clone https://github.com/SpareCores/sc-membench
cd sc-membench

# Install the compiler and the libraries (Debian/Ubuntu)
sudo apt-get install build-essential libhugetlbfs-dev libhwloc-dev libnuma-dev

# Build (links hwloc, libnuma, and libhugetlbfs when they are installed)
make

# Run with default settings (all CPUs, cache-aware sizes)
./membench

# Log to stderr, and stop after about 5 minutes
./membench -v -t 300

# Test one per-thread buffer size (1,024 KiB)
./membench -s 1024

# Build with hwloc and libnuma (recommended for servers), and use huge pages
make full
./membench-full -H -v
```

See [Building From Source](./docs/build.md) for the other platforms and
build targets, and [Usage Examples](./docs/usage.md#usage-examples) for more
commands.

Production runs the container privileged and with host networking, and passes
`-Hv -t 1800` for huge pages, a verbose log, and a soft time limit of
1,800 seconds:

```bash
docker run --rm --privileged --network host ghcr.io/sparecores/membench:main -Hv -t 1800 > stdout 2> stderr
```

See [Production Setup](./docs/usage.md#production-setup) before comparing
results with the Navigator's.

Production runs of version 1.2.2 took 28 seconds at the fastest, 2 minutes
11 seconds at the median, and 15 minutes 14 seconds at the slowest; 10% took
more than 5 minutes 21 seconds. See
[Run Duration](./docs/usage.md#run-duration) for what drives the time.

### Settings

This benchmark is configured with command-line options; the few environment
variables that apply are listed under
[Environment Variables](./docs/usage.md#environment-variables). The following
options are the most common; see the
[full option list](./docs/usage.md#options) for all of them and their defaults:

- `-v` logs the detected CPUs, memory, caches, NUMA (Non-Uniform Memory
  Access) nodes, and the total runtime to stderr; `-vv` adds allocation,
  huge page, and pinning lines.
- `-H` uses huge pages for buffers of at least twice the huge page size.
  Production passes it.
- `-t SECONDS` stops the sweep after the first measurement that ends past the
  limit (default: unlimited).
- `-s SIZE_KB` tests one per-thread size in KiB instead of the sweep; `-f`
  extends the sweep with 512 MiB, 1 GiB, and larger sizes up to the
  [sweep limit](./docs/workloads.md#buffer-sizes).
- `-o OP` runs only `read`, `write`, `copy`, or `latency`; repeat it for more
  than one.
- `-p THREADS` fixes the thread count, and `-a` tries 1, 2, 4, and so on up
  to the CPU count and keeps the best result per size.
- `-R` prints a human-readable table, a summary, and scores instead of CSV.

### Results

Rows are printed to stdout as each measurement ends, one per size and
operation, with the aggregate bandwidth of all threads in mebibytes per second
(MiB/s; see [Bandwidth Unit](./docs/limitations.md#bandwidth-unit)) and the latency in
nanoseconds. See [CSV Output](./docs/usage.md#csv-output) for all columns,
[Reading the Results](./docs/usage.md#reading-the-results) for where the cache
levels show, and
[How the Navigator Uses the Results](./docs/usage.md#how-the-navigator-uses-the-results)
for what is published.

For example, a production run on an AWS (Amazon Web Services)
[`c8g.medium`](https://sparecores.com/server/aws/c8g.medium) server with
1 vCPU, 64 KiB of L1d, 2 MiB of L2, and 36 MiB of L3 cache measured 10 sizes
from 32 KiB to 128 MiB in 56 seconds
([recorded output](https://github.com/SpareCores/sc-inspector-data/blob/main/data/aws/c8g.medium/0/membench/stdout)).
The rows of three of its sizes follow: the first size (32 KiB), the L3 size
(36 MiB, rounded to the nearest power of 2, 32 MiB), and the largest size
(4 × L3, rounded to 128 MiB):

```text
size_kb,operation,bandwidth_mb_s,latency_ns,latency_stddev_ns,latency_samples,threads,iterations,elapsed_s
32,read,74949.40,0,0,0,1,359712,0.149981
32,write,80180.14,0,0,0,1,586854,0.228725
32,copy,62132.05,0,0,0,1,361794,0.181968
32,latency,0,2.16,0.02,7,1,7,0.311926
...
32768,read,29529.54,0,0,0,1,216,0.234071
32768,write,65090.45,0,0,0,1,515,0.253186
32768,copy,25811.08,0,0,0,1,193,0.239277
32768,latency,0,88.38,8.50,21,1,21,4.573935
...
131072,read,27008.08,0,0,0,1,52,0.246445
131072,write,64442.73,0,0,0,1,126,0.250269
131072,copy,24466.47,0,0,0,1,47,0.245888
131072,latency,0,110.84,0.41,7,1,7,10.066027
```

<!-- embed -->
[Memory Bandwidth of c8g.medium](https://sparecores.com/embed/server/aws/c8g.medium/bw_mem)

Read bandwidth falls from 74,949 MiB/s (\~73 GiB/s) at 32 KiB to 27,008 MiB/s
(\~26 GiB/s) at 128 MiB, and latency rises from 2.16 ns to 110.84 ns. The 32 MiB latency row stopped at
the 21-sample limit with a CV (Coefficient of Variation) of about 10%, above
the 5% target. The CV is `latency_stddev_ns` divided by `latency_ns`. The
16 MiB row, not shown, stopped there too, at about 16%. See
[Latency Sampling](./docs/limitations.md#latency-sampling).

## Workloads

All tests are in one C file, [`membench.c`](./membench.c). Each size runs
the following operations in this fixed order; `-o` only filters them:

- **[Read](./docs/workloads.md#read):** Every thread XORs all 64-bit words of
  its buffer into eight accumulators, 256 bytes per unrolled loop step.
- **[Write](./docs/workloads.md#write):** Every thread stores a 64-bit
  pattern to all words of its buffer.
- **[Copy](./docs/workloads.md#copy):** Every thread copies its source buffer
  to a destination buffer word by word. The bandwidth counts the buffer size
  once, as `bw_mem` does, not read plus write.
- **[Latency](./docs/workloads.md#latency):** One thread pinned to CPU 0
  follows a random-order chain of 16-byte nodes through a buffer of the
  current size, and the median of 7 to 21 timed samples is reported.

The bandwidth operations run on all online CPUs as OpenMP threads, each with
its own buffer on the NUMA node of the CPU it runs on. Each bandwidth
measurement runs one discarded warmup, then three tries, and the best try is
kept.

See [Buffer Sizes](./docs/workloads.md#buffer-sizes) for how the sizes are
derived from the caches, and
[Threads and Memory Placement](./docs/workloads.md#threads-and-memory-placement)
for the thread count, calibration, and tries.

## Design History

We wrote this benchmark in January 2026 after the `bw_mem` findings described
under [Purpose](#purpose). The first release detected caches with `hwloc`,
derived the sizes from them, pinned threads, bound memory to NUMA nodes, and
matched the copy convention of `bw_mem`. The container image was built from
`images/membench/` in the
[`sc-images` repository](https://github.com/SpareCores/sc-images) from
2026-01-05 until 2026-02-27, when the Dockerfile moved to this repository.

The releases that followed added the following:

- 1.0.1: huge page support.
- 1.0.2: measurements that run until a target time or a maximum iteration
  count.
- 1.0.3: a new latency test adapted from `ram_bench`, plus macOS and BSD
  (Berkeley Software Distribution) support.
- 1.1.0: a rewrite of the bandwidth tests to OpenMP.
- 1.2.0: the human-readable output and scores.
- 1.2.1: a single binary per architecture, built for its baseline instruction
  set.
- 1.2.2: a man page.

Since 1.2.2, only the Docker build and CI changed, so the `:main` image runs
1.2.2 code.

Earlier latency measurements with and without huge pages on an Azure and an
AWS server type are kept with the release details in the
[changelog](./CHANGELOG.md). Earlier versions of these docs did not state
which version and settings produced them.

## References

See the full list of [references](./docs/references.md) used in this
documentation, [Building From Source](./docs/build.md) for the build
dependencies, targets, and packaging status, and the
[contributor guide](./docs/contributors.md) for the tests and the image
build.

Related benchmarks: [STREAM](https://www.cs.virginia.edu/stream/),
[LMbench](https://sourceforge.net/projects/lmbench/), and
[`ram_bench`](https://github.com/emilk/ram_bench).

## License

This project is licensed under the Mozilla Public License 2.0; see
[`LICENSE`](./LICENSE).
