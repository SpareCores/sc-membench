# Workloads

## Buffer Sizes

Every size is the buffer size of *one* thread, so the memory in use is the
size times the thread count, and twice that for copy, which needs a source and
a destination buffer. For example, at 1 MiB with four threads, read and write
use 4 MiB in total, and copy uses 8 MiB.

The default size sweep derives the following sizes from the detected L1d, L2,
and L3 cache sizes ([`membench.c`](../membench.c), `get_sizes`):

| Size | What it shows |
| --- | --- |
| L1d / 2 | The L1d cache |
| 2 × L1d | The transition from L1d to L2 |
| L2 / 2 | The L2 cache |
| L2 | The L2 boundary |
| 2 × L2 | The transition from L2 to L3 |
| L3 / 4 | The L3 cache; tested only if L3 is larger than 4 × L2 |
| L3 / 2 | The L3 cache |
| L3 | The transition from L3 to RAM |
| 2 × L3 | RAM |
| 4 × L3 | RAM |

The labels describe the latency rows and the one-thread bandwidth rows.
A bandwidth row with more threads uses the size times the thread count in
total, so with many threads it reaches RAM at a smaller per-thread size than
the table shows.

Each size is rounded to the nearest power of 2. A size is kept only if it is
larger than the previous one and at most the sweep limit, which is half of
the RAM divided by the thread count.

Of the 2,503
[production runs](./usage.md#production-setup) of all versions with a recorded
output, 2,290 tested all 10 sizes, 73 tested nine, and 140 tested eight.

The following options change the sizes:

- `-f` adds 512 MiB, 1 GiB, and each further doubling up to the sweep limit,
  with at most 18 sizes in total.
- `-s SIZE_KB` replaces the sweep with one size.

The sizes are tested in ascending order, and each size runs read, write,
copy, and latency in that order.

### Cache Detection

The cache sizes come from `hwloc`: the first L1d, L2, and L3 cache object it
reports. The L3 size is therefore the size of one shared L3 cache (one die or
socket), not a per-core share and not the total. When a server has several L3
caches, `-v` prints a note with their count.

For example, an AWS (Amazon Web Services) `m7i.48xlarge` server has two L3
caches, the first of 105 MiB, and its sweep ends at 512 MiB per thread. An AWS
`c8a.metal-48xl` server has 24 L3 caches of 32 MiB each, and its sweep ends at
128 MiB per thread, four times one of them
([recorded output](https://github.com/SpareCores/sc-inspector-data/blob/main/data/aws/c8a.metal-48xl/0/membench/stdout)).

If `hwloc` reports no caches, the sizes fall back to the following defaults:
32 KiB for L1d, 256 KiB for L2, and 8 MiB for L3. The sweep then runs from
16 KiB up to 32 MiB, regardless of the real cache sizes. In production, 17
server types print exactly these defaults. All of them have Ampere Altra-class
Arm CPUs; one is Hetzner Cloud `cax11`
([recorded log](https://github.com/SpareCores/sc-inspector-data/blob/main/data/hcloud/cax11/0/membench/stderr)).

The defaults apply per level ([`membench.c`](../membench.c), `use_defaults`): when
only the L3 size is missing, the L3 size becomes 8 MiB and the other sizes stay
as detected.

A binary built without `hwloc` reads the caches of `cpu0` from
`/sys/devices/system/cpu/` on Linux and from `sysctl` on macOS and BSD. The
image is built with `hwloc`.

## Threads and Memory Placement

The read, write, and copy operations share the following method
([`membench.c`](../membench.c), `run_benchmark_omp` and `find_best_config`).

### Thread Count

By default, the thread count is the number of online CPUs.
The `-p` option fixes it, and `-a` tries 1, 2, 4, and so on up to the CPU
count and keeps the count with the highest bandwidth. The CPU count and the
RAM size are those of the host; see
[Container Limits](./limitations.md#container-limits).

### Memory Guard

The thread count is halved until the size times the thread
count (times two for copy) is at most a quarter of the RAM. With `-a`, thread
counts that would exceed it are skipped. The guard is stricter than the
[sweep limit](#buffer-sizes), so a size between the two stays in the sweep
and runs on fewer threads. For example, on `m7i.48xlarge`, copy at
512 MiB ran on 96 of the 192 threads
([recorded output](https://github.com/SpareCores/sc-inspector-data/blob/main/data/aws/m7i.48xlarge/0/membench/stdout)).
The `threads` column shows the count each row used.

### Placement

The threads run in an OpenMP parallel region with the
`proc_bind(spread)` clause. Each thread allocates its own buffer inside that
region and writes to all of it before the measurement. With `libnuma`
available, as in the image, the buffer is allocated on the NUMA
(Non-Uniform Memory Access) node of the CPU that the thread runs on. The
`OMP_NUM_THREADS` and `OMP_PROC_BIND` environment variables do not change
this; see [Environment Variables](./usage.md#environment-variables).

The `spread` policy is meant to distribute the threads evenly across the NUMA
nodes and physical cores, so that every memory controller is used and the
scheduler does not move threads between cores. The NUMA-local buffers avoid
cross-node accesses and page migrations during the measurement. Because no
two threads share a buffer, they do not contend for the same cache lines. On
a single-node host, the NUMA allocation makes no difference.

### Calibration

One warmup read and one timed one-thread pass on the buffer of thread 0 set
the iteration count, the number of passes over the buffer, so that the passes
would take about 0.25 seconds on one thread. The count is kept between 3 and
10,000,000 iterations. All threads then run that count.

### Tries

Each measurement runs once as a discarded warmup, then `-r` times
(default: 3), and the highest bandwidth is kept. The warmup lets the CPU
frequency ramp up. Every try allocates its buffers again.

### Bandwidth

The reported value is the per-thread size times the thread
count times the iteration count, divided by the elapsed time of the slowest
thread. It is the aggregate of all threads, not a per-thread value; see
[Bandwidth Unit](./limitations.md#bandwidth-unit) for its unit.

The loops are plain C, built with `-O3` for the baseline instruction set of
the architecture; see [Plain C Loops](./limitations.md#plain-c-loops).

## Read

Every thread reads all 64-bit words of its buffer and XORs them into eight
independent accumulators, 256 bytes per unrolled loop step
([`membench.c`](../membench.c), `mem_read`).

## Write

Every thread stores one 64-bit pattern to all words of its buffer
([`membench.c`](../membench.c), `mem_write`).

## Copy

Every thread copies its source buffer to its destination buffer one 64-bit
word at a time ([`membench.c`](../membench.c), `mem_copy`). The bandwidth
counts the buffer size once per iteration, as `bw_mem` does, not the bytes read
plus the bytes written. Because each iteration both
reads and writes the buffer size, copy bandwidth is typically lower than read
or write bandwidth.

## Latency

The latency test measures the time of one dependent load: a memory access
whose address comes from the previous access, so that the CPU cannot overlap
or prefetch the accesses. The method is pointer chasing, based on
`ram_bench` by Emil Ernerfeldt ([`membench.c`](../membench.c),
`measure_latency_stats`), and works as follows:

1. Pin the thread to CPU 0, so that the scheduler cannot move it during the
   measurement. On a host with more than one NUMA node, bind the memory to
   the node of CPU 0.
2. Fill a buffer of the current size with nodes of 16 bytes (`LatencyNode`),
   each a payload and a pointer to the next node, with at least 64 nodes.
3. Link the nodes in random order into one cycle that visits every node, so
   that hardware prefetchers cannot predict the next address.
4. Traverse the chain once as a warmup, and once more to calibrate how many
   traversals (1 to 10,000) fill a sample of about 0.1 seconds.
5. Take timed samples until their CV (Coefficient of Variation) is below 5%
   after at least 7 samples, or until 21 samples are taken.
6. Report the median of the samples in nanoseconds per access, with their
   standard deviation and count.

At small sizes, this measures the hit latency of the L1d, L2, and L3 caches,
and at large sizes the RAM latency. Latency runs on one thread and is
measured once per size: apart from the warmup traversal in step 4, it has no
discarded warmup try, unlike the bandwidth operations, and `-r` does not
apply. The order of the nodes differs between runs, because the shuffle is
seeded from the time.

The chain is capped at 2 GiB or a quarter of the RAM, whichever is smaller,
and the row still prints the size of the sweep; see
[Latency Sampling](./limitations.md#latency-sampling). The output does not
mark a capped row: it is capped when `size_kb` exceeds 2 GiB or a quarter of
the RAM. The 2 GiB cap is set to exceed the largest L3 caches, which pass 1 GiB
on some CPUs, so that the chain can still reach RAM. The thread stays pinned to CPU 0 after the test;
see [Thread Placement](./limitations.md#thread-placement).

## Huge Pages

With `-H`, a buffer of at least twice the huge page size is allocated in the
following order:

1. explicit huge pages (`MAP_HUGETLB`)
2. THP (Transparent Huge Pages), requested with `MADV_HUGEPAGE`
3. regular pages

Smaller buffers always use regular pages. In the image, the huge page size
is read from `Hugepagesize` in `/proc/meminfo`, and is 2 MiB if that fails,
which makes the threshold 4 MiB. A binary built with `libhugetlbfs` asks
that library for the size first.

THP needs no pre-allocated pool, but the host's THP mode must be `madvise`
or `always`, and the tool does not check it. To use explicit huge pages, see
[Explicit Huge Pages](./usage.md#explicit-huge-pages).

In the image, `-H` covers the latency chain and bandwidth rows whose
`threads` value is 1. Bandwidth rows with more threads allocate their buffers
through `libnuma`, which bypasses the huge page path; see
[Huge Pages Scope](./limitations.md#huge-pages-scope).

Huge pages matter most for latency: the random accesses of the pointer chase
miss the TLB (Translation Lookaside Buffer) far more often with regular
pages. Production passes `-H`. Earlier measurements with and without it are
in the changelog, under
[Huge Pages and Latency](../CHANGELOG.md#huge-pages-and-latency).

## Comparison With LMbench

The bandwidth and latency tests follow `bw_mem` and `lat_mem_rd` of
[LMbench](https://sourceforge.net/projects/lmbench/), with the following
differences:

| Aspect | `membench` | LMbench |
| --- | --- | --- |
| Parallelism | OpenMP threads, one buffer each | Processes (`fork`), one buffer each |
| Size reported | Per-thread buffer size | Per-process buffer size |
| Read | All of the data (`read`) | `rd` reads every fourth word, 25% of the buffer; `frd` reads all |
| Operations | All in one run; `-o` selects | One invocation per operation, such as `bw_mem 64m rd` and `bw_mem 64m wr` |
| Output | CSV on stdout | Text on stderr |
| Huge pages | `-H` | None (uses `valloc`) |
| Latency chain | 16-byte nodes (payload and pointer) in random order, visiting all nodes | 8-byte pointers at a fixed backward stride, 64 bytes by default on 64-bit |
| Latency result | Median of 7 to 21 samples, with standard deviation and count | One measurement |
| Pinning | Latency on CPU 0, with NUMA-local memory | None |

Both report copy as the buffer size divided by the time.

Both latency tests use dependent loads, which prevent pipelining. A fixed
stride can be detected by stride prefetchers: the `lat_mem_rd` man page calls
the test "vulnerable to smart, stride-sensitive cache prefetching policies".
The random order of the `membench` chain avoids that.
